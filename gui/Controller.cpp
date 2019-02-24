#include "gui/Controller.h"
#include "gui/Config.h"
#include "gui/Factory.h"
#include "gui/MainLoop.h"
#include "gui/Utils.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/Movie.h"
#include "gui/renderers/MeshRenderer.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/windows/MainWindow.h"
#include "gui/windows/OrthoPane.h"
#include "run/IRun.h"
#include "system/Statistics.h"
#include "system/Timer.h"
#include "thread/CheckFunction.h"
#include "thread/Pool.h"
#include <wx/app.h>
#include <wx/dcmemory.h>
#include <wx/msgdlg.h>

NAMESPACE_SPH_BEGIN

static wxWindow* sWindow = nullptr;

Controller::Controller(const GuiSettings& settings, AutoPtr<IPluginControls>&& plugin)
    : plugin(std::move(plugin)) {
    CHECK_FUNCTION(CheckFunction::ONCE);

    // copy settings
    gui = settings;

    // create objects for drawing particles
    vis.initialize(gui);

    // create main frame of the application
    window = alignedNew<MainWindow>(this, gui, this->plugin.get());
    window->SetAutoLayout(true);
    window->Show();

    sWindow = window.get();

    this->startRenderThread();
}

Controller::~Controller() = default;

Controller::Vis::Vis() {
    bitmap = makeAuto<wxBitmap>();
    needsRefresh = false;
    refreshPending = false;
}

void Controller::Vis::initialize(const GuiSettings& gui) {
    /// \todo add Factory::getScheduler and use it here!
    renderer = Factory::getRenderer(*ThreadPool::getGlobalInstance(), gui);
    colorizer = Factory::getColorizer(gui, ColorizerId::VELOCITY);
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

void Controller::start(AutoPtr<IRun>&& run) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    // stop the current one
    this->stop(true);

    // update the status
    status = RunStatus::RUNNING;

    // create and start the run
    sph.run = std::move(run);
    this->startRunThread();
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

        // notify the window
        window->runStarted();
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
        } else {
            ASSERT(path.extension() == Path("txt"));
            Flags<OutputQuantityFlag> flags =
                OutputQuantityFlag::INDEX | OutputQuantityFlag::POSITION | OutputQuantityFlag::VELOCITY;
            output = makeAuto<TextOutput>(path, "unnamed", flags);
        }

        output->dump(storage, stats);
    };

    if (status == RunStatus::RUNNING) {
        // cannot directly access the storage during the run, execute it on the time step
        sph.onTimeStepCallbacks->push(dump);
    } else {
        // if not running, we can safely save the storage from main thread
        /// \todo somehow remember the time of the last simulation?
        Statistics stats;
        stats.set(StatisticsId::RUN_TIME, 0._f);
        stats.set(StatisticsId::TIMESTEP_VALUE, 0.1_f);
        dump(*sph.run->getStorage(), stats);
    }
}

void Controller::loadState(const Path& path) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    // stop the current run
    this->stop(true);

    // update the status
    status = RunStatus::RUNNING;

    // start the run from file
    this->startRunThread(path);
}

void Controller::quit() {
    if (status == RunStatus::QUITTING) {
        // already quitting
        return;
    }
    CHECK_FUNCTION(CheckFunction::ONCE | CheckFunction::MAIN_THREAD);

    // set status so that other threads know to quit
    status = RunStatus::QUITTING;

    // unpause run
    continueVar.notify_one();

    wxTheApp->ProcessPendingEvents();

    // wait for the run to finish
    if (sph.thread.joinable()) {
        sph.thread.join();
    }

    // close animation object
    movie.reset();

    // kill the window, this will not trigger another window close event
    window->Destroy();

    // yield to process the event
    wxYield();
}

void Controller::onTimeStep(const Storage& storage, Statistics& stats) {
    ASSERT(std::this_thread::get_id() == sph.thread.get_id());
    if (status == RunStatus::QUITTING) {
        return;
    }

    // update run progress
    const float progress = stats.get<Float>(StatisticsId::RELATIVE_PROGRESS);
    executeOnMainThread([this, progress] { window->setProgress(progress); });

    // update the data in all window controls (can be done from any thread)
    window->onTimeStep(storage, stats);

    // check current time and possibly save images
    ASSERT(movie);
    movie->onTimeStep(storage, stats);

    // executed all waiting callbacks (before redrawing as it is used to change renderers)
    if (!sph.onTimeStepCallbacks->empty()) {
        auto onTimeStepProxy = sph.onTimeStepCallbacks.lock();
        for (auto& func : onTimeStepProxy.get()) {
            func(storage, stats);
        }
        onTimeStepProxy->clear();
    }

    // update the data for rendering
    if (vis.timer->isExpired()) {
        this->redraw(storage, stats);
        vis.timer->restart();

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
}

void Controller::onRunFailure(const DiagnosticsError& error, const Statistics& stats) {
    ASSERT(std::this_thread::get_id() == sph.thread.get_id());
    window->onRunFailure(error, stats);
}

GuiSettings& Controller::getParams() {
    return gui;
}

void Controller::setParams(const GuiSettings& settings) {
    // CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    gui = settings;
    // reset camera
    const Pixel size(gui.get<int>(GuiSettingsId::VIEW_WIDTH), gui.get<int>(GuiSettingsId::VIEW_HEIGHT));
    std::unique_lock<std::mutex> cameraLock(vis.cameraMutex);
    vis.camera = Factory::getCamera(gui, size);
    // reset renderer with new params
    this->setRenderer(Factory::getRenderer(*ThreadPool::getGlobalInstance(), gui));
}

void Controller::update(const Storage& storage) {
    // fill the combobox with available colorizer
    executeOnMainThread([this, &storage] { //
        window->runStarted();
        window->setColorizerList(this->getColorizerList(storage, false));
        /// \todo probably not the best place for this
        if (plugin) {
            plugin->statusChanges(status);
        }

        /// \todo wait till completed!!!
    });

    // draw initial positions of particles
    /// \todo generalize stats
    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    this->redraw(storage, stats);

    // set up animation object
    movie = this->createMovie(storage);

    sph.shouldContinue = true;
}

bool Controller::shouldAbortRun() const {
    return !sph.shouldContinue;
}

bool Controller::isQuitting() const {
    return status == RunStatus::QUITTING;
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
        colorizerIds.push(ColorizerId::PARTICLE_ID);
        colorizerIds.push(ColorizerId::COMPONENT_ID);
        colorizerIds.push(ColorizerId::MARKER);

        if (storage.has(QuantityId::ENERGY)) {
            colorizerIds.push(ColorizerId::TEMPERATURE);
        }

        if (storage.getUserData()) {
            colorizerIds.push(ColorizerId::AGGREGATE_ID);
        }

        colorizerIds.push(ColorizerId::FLAG);

        if (storage.has(QuantityId(GuiQuantityId::UVW))) {
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
        quantityColorizerIds.push(QuantityId::ANGULAR_FREQUENCY);
        quantityColorizerIds.push(QuantityId::MOMENT_OF_INERTIA);
        quantityColorizerIds.push(QuantityId::STRAIN_RATE_CORRECTION_TENSOR);
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
        colorizers.push(Factory::getColorizer(gui, ColorizerId::VELOCITY));
    }

    // add all quantity colorizers (sorted by the key)
    std::sort(quantityColorizerIds.begin(), quantityColorizerIds.end());
    for (QuantityId id : quantityColorizerIds) {
        if (storage.has(id)) {
            colorizers.push(Factory::getColorizer(gui, ColorizerId(id)));
        }
    }

    // add all auxiliary colorizers (sorted by the key)
    std::sort(colorizerIds.begin(), colorizerIds.end());
    for (ColorizerId id : colorizerIds) {
        colorizers.push(Factory::getColorizer(gui, id));
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

    const float radius = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
    const CameraRay ray = camera->unproject(Coords(position));
    const float cutoff = camera->getCutoff().valueOr(0.f);
    const Vector dir = getNormalized(ray.target - ray.origin);

    struct {
        float t = -INFTY;
        Size idx = -1;
        bool wasHitOutside = true;
    } first;

    for (Size i = 0; i < vis.positions.size(); ++i) {
        Optional<ProjectedPoint> p = camera->project(vis.positions[i]);
        if (!p) {
            // particle not visible by the camera
            continue;
        }
        if (cutoff != 0._f && abs(dot(camera->getDirection(), vis.positions[i])) > cutoff) {
            // particle cut off by projection
            /// \todo This is really weird, we are duplicating code of ParticleRenderer in a function that
            /// really makes only sense with ParticleRenderer. Needs refactoring.
            continue;
        }

        const Vector r = vis.positions[i] - ray.origin;
        const float t = dot(r, dir);
        const Vector projected = r - t * dir;
        /// \todo this radius computation is actually renderer-specific ...
        const float radiusSqr = sqr(vis.positions[i][H] * radius);
        const float distanceSqr = getSqrLength(projected);
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
    this->tryRedraw();

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
        Statistics dummy;
        func(*sph.run->getStorage(), dummy);
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
            window->setSelectedParticle(particle.value(), color);
            return;
        }
    }

    window->deselectParticle();
}

void Controller::setPaletteOverride(const Palette palette) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);

    Config::setPalette(vis.colorizer->name(), palette);
    vis.colorizer->setPalette(palette);
    this->tryRedraw();
}

Optional<Size> Controller::getSelectedParticle() const {
    return vis.selectedParticle;
}

SharedPtr<Movie> Controller::createMovie(const Storage& storage) {
    ASSERT(sph.run);
    RenderParams params;
    params.particles.scale = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
    params.size.x = gui.get<int>(GuiSettingsId::IMAGES_WIDTH);
    params.size.y = gui.get<int>(GuiSettingsId::IMAGES_HEIGHT);

    Array<SharedPtr<IColorizer>> colorizers;
    GuiSettings guiClone = gui;
    guiClone.set(GuiSettingsId::RENDERER, gui.get<RendererEnum>(GuiSettingsId::IMAGES_RENDERER))
        .set(GuiSettingsId::RAYTRACE_SUBSAMPLING, 1);
    AutoPtr<IRenderer> renderer = Factory::getRenderer(*ThreadPool::getGlobalInstance(), guiClone);
    // limit colorizer list for slower renderers
    switch (gui.get<RendererEnum>(GuiSettingsId::IMAGES_RENDERER)) {
    case RendererEnum::PARTICLE:
        colorizers = this->getColorizerList(storage, true);
        break;
    case RendererEnum::MESH:
        colorizers = { Factory::getColorizer(gui, ColorizerId::VELOCITY) };
        break;
    case RendererEnum::RAYTRACER:
        colorizers = { Factory::getColorizer(gui, ColorizerId::VELOCITY) };
        break;
    default:
        STOP;
    }
    const Pixel size(gui.get<int>(GuiSettingsId::IMAGES_WIDTH), gui.get<int>(GuiSettingsId::IMAGES_HEIGHT));
    params.camera = Factory::getCamera(gui, size);
    return makeShared<Movie>(gui, std::move(renderer), std::move(colorizers), std::move(params));
}

void Controller::redraw(const Storage& storage, Statistics& stats) {
    CHECK_FUNCTION(CheckFunction::NON_REENRANT);

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
    vis.camera->initialize(storage);
    AutoPtr<ICamera> camera = vis.camera->clone();
    vis.cameraMutex.unlock();

    // update the renderer with new data
    vis.renderer->initialize(storage, *vis.colorizer, *camera);

    // notify the render thread that new data are available
    vis.refresh();
}

void Controller::tryRedraw() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    if (status != RunStatus::RUNNING) {
        // we can safely access the storage
        ASSERT(sph.run);
        SharedPtr<Storage> storage = sph.run->getStorage();
        if (!storage) {
            return;
        }

        vis.renderer->cancelRender();
        std::unique_lock<std::mutex> renderLock(vis.renderThreadMutex);
        vis.colorizer->initialize(*storage, RefEnum::STRONG);

        vis.cameraMutex.lock();
        AutoPtr<ICamera> camera = vis.camera->clone();
        vis.cameraMutex.unlock();
        vis.renderer->initialize(*storage, *vis.colorizer, *camera);
        vis.timer->restart();
        vis.refresh();
    }
}

void Controller::refresh(AutoPtr<ICamera>&& camera) {
    // invalidate camera, render will be restarted on next timestep
    vis.renderer->cancelRender();
    std::unique_lock<std::mutex> lock(vis.cameraMutex);
    vis.camera = std::move(camera);
    vis.refresh();
}

void Controller::startRunThread(const Path& path) {
    sph.thread = std::thread([this, path] {
        sph.shouldContinue = true;

        try {
            // create storage and set up initial conditions
            sph.run->setUp();
        } catch (std::exception& e) {
            executeOnMainThread([desc = std::string(e.what())] { //
                wxMessageBox(std::string("Invalid run setup: \n") + desc, "Fail", wxOK | wxCENTRE);
            });
            return;
        }

        SharedPtr<Storage> storage = sph.run->getStorage();

        // if we want to resume run from state file, load the storage
        if (!path.empty()) {
            ASSERT(path.extension() == Path("ssf"));
            BinaryInput input;
            Statistics stats;
            Outcome result = input.load(path, *storage, stats);
            if (!result) {
                executeOnMainThread([result] {
                    wxMessageBox("Cannot resume the run: " + result.error(), "Error", wxOK | wxCENTRE);
                });
                return;
            }
        }

        // setup image output, generate colorizers, etc.
        this->update(*storage);

        // run the simulation
        sph.run->run();

        // set status to finished
        status = RunStatus::STOPPED;
    });
}

void Controller::startRenderThread() {
    class RenderOutput : public IRenderOutput {
    private:
        Vis& vis;
        RawPtr<MainWindow> window;

    public:
        RenderOutput(Vis& vis, RawPtr<MainWindow> window)
            : vis(vis)
            , window(window) {}

        virtual void update(const Bitmap<Rgba>& bitmap, Array<Label>&& labels) override {
            if (vis.refreshPending) {
                return;
            }

            auto callback = [this, bitmap = bitmap.clone(), labels = std::move(labels)] {
                toWxBitmap(bitmap, *vis.bitmap);
                wxMemoryDC dc(*vis.bitmap);
                printLabels(dc, labels);
                dc.SelectObject(wxNullBitmap);
                window->Refresh();
                vis.refreshPending = true;
            };

            executeOnMainThread(std::move(callback));
        }
    };

    vis.renderThread = std::thread([this] {
        RenderOutput output(vis, window);
        while (status != RunStatus::QUITTING) {

            // wait till render data are available
            std::unique_lock<std::mutex> renderLock(vis.renderThreadMutex);
            vis.renderThreadVar.wait(renderLock, [this] { return vis.needsRefresh.load(); });
            vis.needsRefresh = false;
            ASSERT(vis.isInitialized());

            const wxSize canvasSize = window->getCanvasSize();
            RenderParams params;
            params.size = Pixel(canvasSize.x, canvasSize.y);
            params.particles.scale = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
            params.particles.selected = vis.selectedParticle;
            params.particles.grayScale = gui.get<bool>(GuiSettingsId::FORCE_GRAYSCALE);
            params.surface.level = gui.get<Float>(GuiSettingsId::SURFACE_LEVEL);
            params.surface.ambientLight = gui.get<Float>(GuiSettingsId::SURFACE_AMBIENT);
            params.surface.sunLight = gui.get<Float>(GuiSettingsId::SURFACE_SUN_INTENSITY);

            vis.cameraMutex.lock();
            params.camera = vis.camera->clone();
            vis.cameraMutex.unlock();

            vis.renderer->render(params, *vis.stats, output);
        }
    });
}

NAMESPACE_SPH_END
