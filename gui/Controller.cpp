#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/MainLoop.h"
#include "gui/Project.h"
#include "gui/Utils.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/Movie.h"
#include "gui/renderers/MeshRenderer.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/windows/RunPage.h"
#include "run/Node.h"
#include "run/workers/IoJobs.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "system/Timer.h"
#include "thread/CheckFunction.h"
#include "thread/Pool.h"
#include <wx/app.h>
#include <wx/checkbox.h>
#include <wx/dcmemory.h>
#include <wx/msgdlg.h>

NAMESPACE_SPH_BEGIN

Controller::Controller(wxWindow* parent)
    : project(Project::getInstance()) {

    // create objects for drawing particles
    vis.initialize(project);

    // create associated page
    GuiSettings& gui = project.getGuiSettings();
    page = alignedNew<RunPage>(parent, this, gui);

    this->startRenderThread();
}

Controller::~Controller() {
    this->quit(true);
}

RawPtr<RunPage> Controller::getPage() const {
    return page;
}

Controller::Vis::Vis() {
    bitmap = makeAuto<wxBitmap>();
    needsRefresh = false;
    refreshPending = false;
    redrawOnNextTimeStep = false;
}

void Controller::Vis::initialize(const Project& project) {
    const GuiSettings& gui = project.getGuiSettings();

    renderer = Factory::getRenderer(gui);
    colorizer = Factory::getColorizer(project, ColorizerId::VELOCITY);
    timer = makeAuto<Timer>(gui.get<int>(GuiSettingsId::VIEW_MAX_FRAMERATE), TimerFlags::START_EXPIRED);
    const Pixel size(gui.get<int>(GuiSettingsId::VIEW_WIDTH), gui.get<int>(GuiSettingsId::VIEW_HEIGHT));
    camera = Factory::getCamera(gui, size);
}

bool Controller::Vis::isInitialized() {
    return renderer && stats && colorizer && camera;
}

void Controller::Vis::refresh() {
    needsRefresh = true;
    renderThreadVar.notify_one();
}

void Controller::start(SharedPtr<JobNode> run, const RunSettings& globals) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
    // sanity check that we don't override ALL the settings; increase if necessary
    ASSERT(globals.size() < 15);

    // stop the current one
    this->stop(true);

    // update the status
    status = RunStatus::RUNNING;
    sph.shouldContinue = true;

    // create and start the run
    sph.globals = globals;
    sph.run = std::move(run);
    this->startRunThread();
}

void Controller::open(const Path& path, const bool sequence) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
    ASSERT(!path.empty());
    sph.path = path;
    page->showTimeLine(true);

    if (sequence) {
        this->start(makeNode<FileSequenceJob>("loader", path), EMPTY_SETTINGS);
    } else {
        this->start(makeNode<LoadFileJob>(path), EMPTY_SETTINGS);
    }
}

void Controller::restart() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    switch (status) {
    case RunStatus::RUNNING:
    case RunStatus::QUITTING:
        // already running or shutting down, do nothing
        return;
    case RunStatus::PAUSED:
        // unpause
        continueVar.notify_one();
        break;
    case RunStatus::STOPPED:
        // wait till previous run finishes
        if (sph.thread.joinable()) {
            sph.thread.join();
        }
        // start new simulation
        this->startRunThread();
    }
    status = RunStatus::RUNNING;
}

void Controller::pause() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    status = RunStatus::PAUSED;
}

void Controller::stop(const bool waitForFinish) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);

    sph.shouldContinue = false;

    // notify continue CV to unpause run (if it's paused), otherwise we would get deadlock
    continueVar.notify_one();

    if (waitForFinish && sph.thread.joinable()) {
        sph.thread.join();
        ASSERT(status == RunStatus::STOPPED);
    }
}

void Controller::saveState(const Path& path) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    auto dump = [path](const Storage& storage, const Statistics& stats) {
        AutoPtr<IOutput> output;
        if (path.extension() == Path("ssf")) {
            /// \todo run type!
            output = makeAuto<BinaryOutput>(path);
        } else if (path.extension() == Path("scf")) {
            output = makeAuto<CompressedOutput>(path, CompressionEnum::RLE, RunTypeEnum::SPH);
        } else if (path.extension() == Path("vtu")) {
            Flags<OutputQuantityFlag> flags = OutputQuantityFlag::VELOCITY | OutputQuantityFlag::DENSITY |
                                              OutputQuantityFlag::ENERGY | OutputQuantityFlag::DAMAGE |
                                              OutputQuantityFlag::SMOOTHING_LENGTH;
            output = makeAuto<VtkOutput>(path, flags);
        } else {
            ASSERT(path.extension() == Path("txt"));
            Flags<OutputQuantityFlag> flags = OutputQuantityFlag::INDEX | OutputQuantityFlag::POSITION |
                                              OutputQuantityFlag::VELOCITY | OutputQuantityFlag::MASS;
            output = makeAuto<TextOutput>(path, "unnamed", flags);
        }

        Expected<Path> result = output->dump(storage, stats);
        if (!result) {
            wxMessageBox("Cannot save the file.\n\n" + result.error(), "Fail", wxOK | wxCENTRE);
        }
    };

    if (status == RunStatus::RUNNING) {
        // cannot directly access the storage during the run, execute it on the time step
        sph.onTimeStepCallbacks->push(dump);
    } else if (sph.storage) {
        // if not running, we can safely save the storage from main thread
        BusyCursor wait(page->GetGrandParent());
        dump(*sph.storage, *vis.stats);
    }
}

void Controller::quit(const bool waitForFinish) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);

    // set status so that other threads know to quit
    status = RunStatus::QUITTING;
    sph.shouldContinue = false; /// \todo is this necessary? why not just use status?

    // unpause run
    continueVar.notify_one();

    // wait for the run to finish
    if (waitForFinish) {
        if (sph.thread.joinable()) {
            sph.thread.join();
        }

        {
            std::unique_lock<std::mutex> renderLock(vis.renderThreadMutex);
            vis.renderThreadVar.notify_one();
        }

        if (vis.renderThread.joinable()) {
            vis.renderThread.join();
        }
    }

    // close animation object
    movie.reset();
}

void Controller::setAutoZoom(const bool enable) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    GuiSettings& gui = project.getGuiSettings();
    if (gui.get<bool>(GuiSettingsId::CAMERA_AUTOSETUP) == enable) {
        return;
    }
    gui.set(GuiSettingsId::CAMERA_AUTOSETUP, enable);
    wxWindow* window = wxWindow::FindWindowByLabel("Auto-zoom", page.get());
    ASSERT(window != nullptr);
    wxCheckBox* checkbox = dynamic_cast<wxCheckBox*>(window);
    checkbox->SetValue(enable);
}

void Controller::onSetUp(const Storage& storage, Statistics& stats) {
    sph.storage = &storage;
    this->update(storage, stats);
}

void Controller::onStart(const IJob& job) {
    const std::string className = job.className();
    const std::string instanceName = job.instanceName();
    this->safePageCall([className, instanceName](RunPage* page) { page->newPhase(className, instanceName); });
}


void Controller::onEnd(const Storage& storage, const Statistics& stats) {
    if (!storage.empty()) {
        sph.storage = &storage;

        if (sph.shouldContinue) {
            // If not continuing, we might be already waiting for next run, i.e. we cannot block main thread
            // and wait for signal. If the update is necessary, we have to introduce additional flag
            // (sph.runPending)
            this->update(storage, stats);
        }
    }

    Statistics endStats = stats;
    endStats.set(StatisticsId::RELATIVE_PROGRESS, 1._f);

    this->safePageCall([endStats](RunPage* page) {
        page->setProgress(endStats);
        page->onRunEnd();
    });
}

void Controller::onTimeStep(const Storage& storage, Statistics& stats) {
    // ASSERT(std::this_thread::get_id() == sph.thread.get_id()); - can be actually called from worker thread,
    // but that should not matter, as long as there is not a concurrent access from different thread

    if (status == RunStatus::QUITTING) {
        return;
    }

    Timer timer;

    // update run progress
    this->safePageCall([stats](RunPage* page) {
        /// \todo remove, keep just onTimeStep
        page->setProgress(stats);
    });

    if (storage.empty()) {
        return;
    }

    // update the data in all window controls (can be done from any thread)
    page->onTimeStep(storage, stats);

    // check current time and possibly save images
    ASSERT(movie);
    movie->onTimeStep(storage, stats);

    // executed all waiting callbacks (before redrawing as it is used to change renderers)
    if (!sph.onTimeStepCallbacks->empty()) {
        MEASURE_SCOPE("onTimeStep - plots");
        auto onTimeStepProxy = sph.onTimeStepCallbacks.lock();
        for (auto& func : onTimeStepProxy.get()) {
            func(storage, stats);
        }
        onTimeStepProxy->clear();
    }

    // update the data for rendering
    const GuiSettings& gui = project.getGuiSettings();
    const bool doRedraw = vis.redrawOnNextTimeStep || gui.get<bool>(GuiSettingsId::REFRESH_ON_TIMESTEP);
    if (doRedraw && vis.timer->isExpired()) {
        this->redraw(storage, stats);
        vis.timer->restart();
        vis.redrawOnNextTimeStep = false;

        executeOnMainThread([this] {
            // update particle probe - has to be done after we redraw the image as it initializes
            // the colorizer
            this->setSelectedParticle(vis.selectedParticle);
        });
    }

    // pause if we are supposed to
    if (status == RunStatus::PAUSED) {
        std::unique_lock<std::mutex> lock(continueMutex);
        continueVar.wait(lock);
    }

    stats.set(StatisticsId::POSTPROCESS_EVAL_TIME, int(timer.elapsed(TimerUnit::MILLISECOND)));
}

GuiSettings& Controller::getParams() {
    return project.getGuiSettings();
}

void Controller::update(const Storage& storage, const Statistics& stats) {
    CHECK_FUNCTION(CheckFunction::NO_THROW);

    std::unique_lock<std::mutex> lock(updateMutex);
    executeOnMainThread([this, &storage] { //
        std::unique_lock<std::mutex> lock(updateMutex);
        page->runStarted(storage, sph.path);

        // fill the combobox with available colorizer
        page->setColorizerList(this->getColorizerList(storage, false));
        updateVar.notify_one();
    });
    updateVar.wait(lock);

    // draw initial positions of particles
    this->redraw(storage, stats);

    // set up animation object
    movie = this->createMovie(storage);
}

bool Controller::shouldAbortRun() const {
    return !sph.shouldContinue;
}

bool Controller::isRunning() const {
    return status == RunStatus::RUNNING;
}

Array<SharedPtr<IColorizer>> Controller::getColorizerList(const Storage& storage, const bool forMovie) const {
    // Available colorizers for display and movie are currently hardcoded
    Array<ColorizerId> colorizerIds;
    Array<QuantityId> quantityColorizerIds;

    /// \todo custom colorizer lists

    colorizerIds.push(ColorizerId::COROTATING_VELOCITY);
    if (storage.has(QuantityId::DENSITY)) {
        colorizerIds.push(ColorizerId::DENSITY_PERTURBATION);
    }
    if (storage.has(QuantityId::DAMAGE)) {
        colorizerIds.push(ColorizerId::DAMAGE_ACTIVATION);
    }
    if (storage.has(QuantityId::PRESSURE) && storage.has(QuantityId::DEVIATORIC_STRESS)) {
        colorizerIds.push(ColorizerId::TOTAL_STRESS);
    }
    if (storage.has(QuantityId::ENERGY)) {
        colorizerIds.push(ColorizerId::TOTAL_ENERGY);
        colorizerIds.push(ColorizerId::BEAUTY);
    }

    if (!forMovie) {
        if (storage.has(QuantityId::MASS)) {
            colorizerIds.push(ColorizerId::SUMMED_DENSITY);
        }
        if (storage.has(QuantityId::STRESS_REDUCING)) {
            colorizerIds.push(ColorizerId::YIELD_REDUCTION);
        }
        colorizerIds.push(ColorizerId::MOVEMENT_DIRECTION);
        colorizerIds.push(ColorizerId::ACCELERATION);
        colorizerIds.push(ColorizerId::RADIUS);
        colorizerIds.push(ColorizerId::DEPTH);
        colorizerIds.push(ColorizerId::PARTICLE_ID);
        colorizerIds.push(ColorizerId::COMPONENT_ID);
        colorizerIds.push(ColorizerId::MARKER);

        if (storage.has(QuantityId::ENERGY) && storage.getMaterialCnt() > 0) {
            colorizerIds.push(ColorizerId::TEMPERATURE);
        }

        if (storage.getUserData()) {
            colorizerIds.push(ColorizerId::AGGREGATE_ID);
        }

        if (storage.has(QuantityId::FLAG)) {
            colorizerIds.push(ColorizerId::FLAG);
        }
        if (storage.has(QuantityId::MATERIAL_ID)) {
            colorizerIds.push(ColorizerId::MATERIAL_ID);
        }

        if (storage.has(QuantityId::UVW)) {
            colorizerIds.push(ColorizerId::UVW);
        }

        if (storage.has(QuantityId::NEIGHBOUR_CNT)) {
            colorizerIds.push(ColorizerId::BOUNDARY);
        }
    }

    quantityColorizerIds.pushAll(Array<QuantityId>{
        QuantityId::PRESSURE,
        QuantityId::ENERGY,
        QuantityId::DEVIATORIC_STRESS,
        QuantityId::DAMAGE,
        QuantityId::VELOCITY_DIVERGENCE,
        QuantityId::FRICTION,
    });

    if (!forMovie) {
        quantityColorizerIds.push(QuantityId::DENSITY);
        quantityColorizerIds.push(QuantityId::MASS);
        quantityColorizerIds.push(QuantityId::AV_ALPHA);
        quantityColorizerIds.push(QuantityId::AV_BALSARA);
        quantityColorizerIds.push(QuantityId::AV_STRESS);
        quantityColorizerIds.push(QuantityId::SOUND_SPEED);
        quantityColorizerIds.push(QuantityId::VIBRATIONAL_VELOCITY);
        quantityColorizerIds.push(QuantityId::BULK_DENSITY);
        quantityColorizerIds.push(QuantityId::ANGULAR_FREQUENCY);
        quantityColorizerIds.push(QuantityId::MOMENT_OF_INERTIA);
        quantityColorizerIds.push(QuantityId::STRAIN_RATE_CORRECTION_TENSOR);
        quantityColorizerIds.push(QuantityId::PHASE_ANGLE);
        quantityColorizerIds.push(QuantityId::EPS_MIN);
        quantityColorizerIds.push(QuantityId::NEIGHBOUR_CNT);
        quantityColorizerIds.push(QuantityId::VELOCITY_GRADIENT);
        quantityColorizerIds.push(QuantityId::VELOCITY_LAPLACIAN);
        quantityColorizerIds.push(QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE);
    }

    Array<SharedPtr<IColorizer>> colorizers;
    // add velocity (always present)
    if (vis.colorizer) {
        IColorizer& colorizer = *vis.colorizer; // clang complains when dereferencing ptr inside typeid
        if (typeid(colorizer) == typeid(VelocityColorizer)) {
            // hack to avoid creating two velocity colorizers (they would have to somehow share the palette
            // ...)
            colorizers.push(vis.colorizer);
        }
    }

    if (colorizers.empty()) {
        colorizers.push(Factory::getColorizer(project, ColorizerId::VELOCITY));
    }

    // add all quantity colorizers (sorted by the key)
    std::sort(quantityColorizerIds.begin(), quantityColorizerIds.end());
    for (QuantityId id : quantityColorizerIds) {
        if (storage.has(id)) {
            colorizers.push(Factory::getColorizer(project, ColorizerId(id)));
        }
    }

    // add all auxiliary colorizers (sorted by the key)
    std::sort(colorizerIds.begin(), colorizerIds.end());
    for (ColorizerId id : colorizerIds) {
        colorizers.push(Factory::getColorizer(project, id));
    }

    return colorizers;
}

const wxBitmap& Controller::getRenderedBitmap() const {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    vis.refreshPending = false;
    return *vis.bitmap;
}

SharedPtr<IColorizer> Controller::getCurrentColorizer() const {
    ASSERT(vis.colorizer != nullptr);
    return vis.colorizer;
}

AutoPtr<ICamera> Controller::getCurrentCamera() const {
    std::unique_lock<std::mutex> cameraLock(vis.cameraMutex);
    return vis.camera->clone();
}

Optional<Size> Controller::getIntersectedParticle(const Pixel position, const float toleranceEps) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);

    if (!vis.colorizer->isInitialized()) {
        // we have to wait for redraw to get data from storage
        return NOTHING;
    }

    std::unique_lock<std::mutex> cameraLock(vis.cameraMutex);
    AutoPtr<ICamera> camera = vis.camera->clone();
    cameraLock.unlock();

    const GuiSettings& gui = project.getGuiSettings();
    const float radius = float(gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS));
    const Optional<CameraRay> ray = camera->unproject(Coords(position));
    if (!ray) {
        return NOTHING;
    }

    const float cutoff = camera->getCutoff().valueOr(0.f);
    const Vector rayDir = getNormalized(ray->target - ray->origin);
    const Vector camDir = camera->getFrame().row(2);

    struct {
        float t = -std::numeric_limits<float>::lowest();
        Size idx = -1;
        bool wasHitOutside = true;
    } first;

    for (Size i = 0; i < vis.positions.size(); ++i) {
        Optional<ProjectedPoint> p = camera->project(vis.positions[i]);
        if (!p) {
            // particle not visible by the camera
            continue;
        }
        if (cutoff != 0._f && abs(dot(camDir, vis.positions[i])) > cutoff) {
            // particle cut off by projection
            /// \todo This is really weird, we are duplicating code of ParticleRenderer in a function that
            /// really makes only sense with ParticleRenderer. Needs refactoring.
            continue;
        }

        const Vector r = vis.positions[i] - ray->origin;
        const float t = float(dot(r, rayDir));
        const Vector projected = r - t * rayDir;
        /// \todo this radius computation is actually renderer-specific ...
        const float radiusSqr = float(sqr(vis.positions[i][H] * radius));
        const float distanceSqr = float(getSqrLength(projected));
        if (distanceSqr < radiusSqr * sqr(1._f + toleranceEps)) {
            const bool wasHitOutside = distanceSqr > radiusSqr;
            // hit candidate, check if it's closer or current candidate was hit outside the actual radius
            if (t < first.t || (first.wasHitOutside && !wasHitOutside)) {
                // update the current candidate
                first.idx = i;
                first.t = t;
                first.wasHitOutside = wasHitOutside;
            }
        }
    }
    if (int(first.idx) == -1) { /// \todo wait, -INFTY != -inf ?? // (first.t == -INFTY) {
        // not a single candidate found
        return NOTHING;
    } else {
        return first.idx;
    }
}

void Controller::setColorizer(const SharedPtr<IColorizer>& newColorizer) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    vis.colorizer = newColorizer;
    if (!this->tryRedraw()) {
        this->redrawOnNextTimeStep();
    }

    // update particle probe with the new colorizer
    this->setSelectedParticle(vis.selectedParticle);
}

void Controller::setRenderer(AutoPtr<IRenderer>&& newRenderer) {
    // CHECK_FUNCTION(CheckFunction::MAIN_THREAD);

    auto func = [this, renderer = std::move(newRenderer)](
                    const Storage& storage, const Statistics& UNUSED(stats)) mutable {
        ASSERT(sph.run);
        std::unique_lock<std::mutex> renderLock(vis.renderThreadMutex);
        vis.renderer = std::move(renderer);
        vis.colorizer->initialize(storage, RefEnum::STRONG);
        vis.renderer->initialize(storage, *vis.colorizer, *vis.camera);
        vis.refresh();
    };

    vis.renderer->cancelRender();
    if (status != RunStatus::RUNNING) {
        // if no simulation is running, it's safe to switch renderers and access storage directly
        func(*sph.storage, *vis.stats);
    } else {
        // if running, we have to switch renderers on next timestep
        sph.onTimeStepCallbacks->push(std::move(func));
    }
}

void Controller::setSelectedParticle(const Optional<Size>& particleIdx) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    vis.selectedParticle = particleIdx;

    /// \todo if the colorizer is not initialized, we should selected the particle after the next timestep
    if (particleIdx && vis.colorizer->isInitialized()) {
        const Rgba color = vis.colorizer->evalColor(particleIdx.value());
        Optional<Particle> particle = vis.colorizer->getParticle(particleIdx.value());
        if (particle) {
            // add position to the particle data
            particle->addValue(QuantityId::POSITION, vis.positions[particleIdx.value()]);
            page->setSelectedParticle(particle.value(), color);
            return;
        }
    }

    page->deselectParticle();
}

void Controller::setPaletteOverride(const Palette palette) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);

    project.setPalette(vis.colorizer->name(), palette);
    vis.colorizer->setPalette(palette);
    this->tryRedraw();
}

Optional<Size> Controller::getSelectedParticle() const {
    return vis.selectedParticle;
}

const Storage& Controller::getStorage() const {
    /// \todo synchronize
    return *sph.storage;
}

SharedPtr<Movie> Controller::createMovie(const Storage& storage) const {
    ASSERT(sph.run);
    RenderParams params;
    const GuiSettings& gui = project.getGuiSettings();
    params.particles.scale = float(gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS));
    params.size.x = gui.get<int>(GuiSettingsId::IMAGES_WIDTH);
    params.size.y = gui.get<int>(GuiSettingsId::IMAGES_HEIGHT);

    Array<SharedPtr<IColorizer>> colorizers;
    GuiSettings guiClone = gui;
    guiClone.accessor = nullptr;
    guiClone.set(GuiSettingsId::RENDERER, gui.get<RendererEnum>(GuiSettingsId::IMAGES_RENDERER))
        .set(GuiSettingsId::RAYTRACE_SUBSAMPLING, 1);
    AutoPtr<IRenderer> renderer = Factory::getRenderer(guiClone);
    // limit colorizer list for slower renderers
    switch (gui.get<RendererEnum>(GuiSettingsId::IMAGES_RENDERER)) {
    case RendererEnum::PARTICLE:
        colorizers = this->getColorizerList(storage, true);
        break;
    case RendererEnum::MESH:
        colorizers = { Factory::getColorizer(project, ColorizerId::VELOCITY) };
        break;
    case RendererEnum::RAYTRACER:
        colorizers = { Factory::getColorizer(project, ColorizerId::VELOCITY) };
        break;
    default:
        STOP;
    }
    const Pixel size(gui.get<int>(GuiSettingsId::IMAGES_WIDTH), gui.get<int>(GuiSettingsId::IMAGES_HEIGHT));
    params.camera = Factory::getCamera(gui, size);

    /// \todo deduplicate - create (fill) params from settings
    params.particles.grayScale = gui.get<bool>(GuiSettingsId::FORCE_GRAYSCALE);
    params.particles.doAntialiasing = gui.get<bool>(GuiSettingsId::ANTIALIASED);
    params.particles.smoothed = gui.get<bool>(GuiSettingsId::SMOOTH_PARTICLES);
    params.surface.level = float(gui.get<Float>(GuiSettingsId::SURFACE_LEVEL));
    params.surface.ambientLight = float(gui.get<Float>(GuiSettingsId::SURFACE_AMBIENT));
    params.surface.sunLight = float(gui.get<Float>(GuiSettingsId::SURFACE_SUN_INTENSITY));

    return makeShared<Movie>(gui, std::move(renderer), std::move(colorizers), std::move(params));
}

void Controller::redraw(const Storage& storage, const Statistics& stats) {
    CHECK_FUNCTION(CheckFunction::NO_THROW);

    vis.renderer->cancelRender();
    std::unique_lock<std::mutex> renderLock(vis.renderThreadMutex);

    vis.stats = makeAuto<Statistics>(stats);
    vis.positions = copyable(storage.getValue<Vector>(QuantityId::POSITION));

    // initialize the currently selected colorizer
    ASSERT(vis.isInitialized());
    vis.colorizer->initialize(storage, RefEnum::STRONG);

    // setup camera
    ASSERT(vis.camera);
    vis.cameraMutex.lock();
    if (project.getGuiSettings().get<bool>(GuiSettingsId::CAMERA_AUTOSETUP)) {
        vis.camera->autoSetup(storage); /// we do autoSetup here AND in orthoPane to get consistent states
        /// \todo can it be done better? (autosetup before passing camera to orthopane?)
    }
    AutoPtr<ICamera> camera = vis.camera->clone();
    vis.cameraMutex.unlock();

    // update the renderer with new data
    vis.renderer->initialize(storage, *vis.colorizer, *camera);

    // notify the render thread that new data are available
    vis.refresh();
}

bool Controller::tryRedraw() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
    if (status != RunStatus::RUNNING && sph.storage && !sph.storage->empty()) {
        vis.renderer->cancelRender();
        std::unique_lock<std::mutex> renderLock(vis.renderThreadMutex);
        vis.colorizer->initialize(*sph.storage, RefEnum::STRONG);

        vis.cameraMutex.lock();
        // vis.camera->initialize(sph.storage);
        AutoPtr<ICamera> camera = vis.camera->clone();
        vis.cameraMutex.unlock();
        vis.renderer->initialize(*sph.storage, *vis.colorizer, *camera);
        vis.timer->restart();
        vis.refresh();

        return true;
    } else {
        vis.renderer->cancelRender();
        vis.refresh();
        return false;
    }
}

void Controller::redrawOnNextTimeStep() {
    vis.redrawOnNextTimeStep = true;
}

void Controller::refresh(AutoPtr<ICamera>&& camera) {
    // invalidate camera, render will be restarted on next timestep
    vis.renderer->cancelRender();
    std::unique_lock<std::mutex> lock(vis.cameraMutex);
    vis.camera = std::move(camera);
    vis.refresh();

    // save the current fov to settings
    /// \todo generalize
    if (const Optional<float> wtp = vis.camera->getWorldToPixel()) {
        const Pixel imageSize = vis.camera->getSize();
        const Float fov = imageSize.y / wtp.value();
        project.getGuiSettings().set(GuiSettingsId::CAMERA_ORTHO_FOV, fov);
    }
}

void Controller::safePageCall(Function<void(RunPage*)> func) {
    executeOnMainThread([func, this] {
        wxWeakRef<RunPage> weakPage = page.get();
        if (weakPage) {
            func(weakPage);
        }
    });
}

void Controller::startRunThread() {
    sph.thread = std::thread([this] {
        sph.shouldContinue = true;

        try {
            // run the simulation
            sph.run->run(sph.globals, *this);

        } catch (std::exception& e) {
            executeOnMainThread([desc = std::string(e.what())] { //
                wxMessageBox(
                    std::string("Error encountered during the run: \n") + desc, "Fail", wxOK | wxCENTRE);
            });
        }

        // set status to finished
        status = RunStatus::STOPPED;
    });
}

void Controller::startRenderThread() {

    class RenderOutput : public IRenderOutput {
    private:
        Vis& vis;
        wxWeakRef<RunPage> page;

    public:
        RenderOutput(Vis& vis, wxWeakRef<RunPage> page)
            : vis(vis)
            , page(page) {}

        virtual void update(const Bitmap<Rgba>& bitmap,
            Array<Label>&& labels,
            const bool UNUSED(isFinal)) override {
            ASSERT(!bitmap.empty());
            if (vis.refreshPending) {
                return;
            }

            /// \todo is it ok to work with wx bitmaps outside main thread?
            AutoPtr<wxBitmap> newBitmap = makeAuto<wxBitmap>();
            toWxBitmap(bitmap, *newBitmap);

            // Capture page as weak ref as we need to check it first, this might not exit anymore!
            auto callback = [&vis = vis,
                                page = page,
                                bitmap = std::move(newBitmap),
                                labels = std::move(labels)]() mutable {
                if (!page) {
                    // page has already closed, nothing to render
                    return;
                }
                vis.refreshPending = true;
                vis.bitmap = std::move(bitmap);

                if (!labels.empty()) {
                    wxMemoryDC dc(*vis.bitmap);
                    printLabels(dc, labels);
                    dc.SelectObject(wxNullBitmap);
                }

                page->refresh();
            };

            executeOnMainThread(std::move(callback));
        }
    };

    vis.renderThread = std::thread([this] {
        RenderOutput output(vis, page.get());
        while (status != RunStatus::QUITTING) {

            // wait till render data are available
            std::unique_lock<std::mutex> renderLock(vis.renderThreadMutex);
            vis.renderThreadVar.wait(renderLock, [this] { //
                return vis.needsRefresh.load() || status == RunStatus::QUITTING;
            });
            vis.needsRefresh = false;

            if (!vis.isInitialized() || status == RunStatus::QUITTING) {
                // no simulation running, go back to sleep
                continue;
            }

            const wxSize canvasSize = page->getCanvasSize();
            RenderParams params;
            params.size = Pixel(canvasSize.x, canvasSize.y);
            params.particles.selected = vis.selectedParticle;

            // initialize all parameters from GUI settings
            params.initialize(project.getGuiSettings());

            vis.cameraMutex.lock();
            params.camera = vis.camera->clone();
            vis.cameraMutex.unlock();

            vis.renderer->render(params, *vis.stats, output);
        }
    });
}

NAMESPACE_SPH_END
