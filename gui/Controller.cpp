#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/MainLoop.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/Movie.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/SurfaceRenderer.h"
#include "gui/windows/MainWindow.h"
#include "gui/windows/OrthoPane.h"
#include "run/IRun.h"
#include "system/Statistics.h"
#include "system/Timer.h"
#include "thread/CheckFunction.h"
#include <wx/app.h>
#include <wx/msgdlg.h>

NAMESPACE_SPH_BEGIN

Controller::Controller(const GuiSettings& settings) {
    CHECK_FUNCTION(CheckFunction::ONCE);

    // copy settings
    gui = settings;

    // create objects for drawing particles
    vis.initialize(gui);

    // create main frame of the application
    window = new MainWindow(this, gui);
    window->SetAutoLayout(true);
    window->Show();
}

Controller::~Controller() = default;

void Controller::Vis::initialize(const GuiSettings& gui) {
    renderer = makeAuto<ParticleRenderer>(gui);
    colorizer = Factory::getColorizer(gui, ColorizerId::VELOCITY);
    timer = makeAuto<Timer>(gui.get<int>(GuiSettingsId::VIEW_MAX_FRAMERATE), TimerFlags::START_EXPIRED);
    const Point size(gui.get<int>(GuiSettingsId::RENDER_WIDTH), gui.get<int>(GuiSettingsId::RENDER_HEIGHT));
    camera = Factory::getCamera(gui, size);
}

bool Controller::Vis::isInitialized() {
    return renderer && stats && colorizer && camera;
}

void Controller::start(AutoPtr<IRun>&& run) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    // stop the current one
    this->stop(true);

    // update the status
    status = Status::RUNNING;

    // create and start the run
    sph.run = std::move(run);
    this->run();
}

void Controller::restart() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    switch (status) {
    case Status::RUNNING:
    case Status::QUITTING:
        // already running or shutting down, do nothing
        return;
    case Status::PAUSED:
        // unpause
        continueVar.notify_one();
        break;
    case Status::STOPPED:
        // wait till previous run finishes
        if (sph.thread.joinable()) {
            sph.thread.join();
        }
        // start new simulation
        this->run();

        // notify the window
        window->runStarted();
    }
    status = Status::RUNNING;
}

void Controller::pause() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    status = Status::PAUSED;
}

void Controller::stop(const bool waitForFinish) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD); // just to make sure there is no race condition on status
    status = Status::STOPPED;
    // notify continue CV to unpause run (if it's paused), otherwise we would get deadlock
    continueVar.notify_one();

    if (waitForFinish && sph.thread.joinable()) {
        sph.thread.join();
    }
}

void Controller::saveState(const Path& path) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    ASSERT(path.extension() == Path("ssf"));
    auto dump = [this, path](const Float time, const Float step) {
        SharedPtr<Storage> storage = sph.run->getStorage();
        BinaryOutput output(path);
        Statistics stats;
        stats.set(StatisticsId::RUN_TIME, time);
        stats.set(StatisticsId::TIMESTEP_VALUE, step);
        output.dump(*storage, stats);
    };

    if (status == Status::RUNNING) {
        // cannot directly access the storage during the run, execute it on the time step
        sph.onTimeStepCallbacks->push(dump);
    } else {
        // if not running, we can safely save the storage from main thread
        /// \todo somehow remember the time of the last simulation?
        dump(0._f, 0.1_f);
    }
}

void Controller::loadState(const Path& path) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    // stop the current run
    this->stop(true);

    // update the status
    status = Status::RUNNING;

    // start the run from file
    this->run(path);
}

void Controller::quit() {
    if (status == Status::QUITTING) {
        // already quitting
        return;
    }
    CHECK_FUNCTION(CheckFunction::ONCE | CheckFunction::MAIN_THREAD);

    // set status so that other threads know to quit
    status = Status::QUITTING;

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
    if (status == Status::QUITTING) {
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

    // update the data for rendering
    if (vis.timer->isExpired()) {
        this->redraw(storage, stats);
        vis.timer->restart();
    }

    // executed all waiting callbacks
    if (!sph.onTimeStepCallbacks->empty()) {
        const Float time = stats.get<Float>(StatisticsId::RUN_TIME);
        const Float step = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
        auto onTimeStepProxy = sph.onTimeStepCallbacks.lock();
        for (auto& func : onTimeStepProxy.get()) {
            func(time, step);
        }
        onTimeStepProxy->clear();
    }

    // pause if we are supposed to
    if (status == Status::PAUSED) {
        std::unique_lock<std::mutex> lock(continueMutex);
        continueVar.wait(lock);
    }
}

GuiSettings& Controller::getParams() {
    return gui;
}

void Controller::setParams(const GuiSettings& settings) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    gui = settings;
    // reset camera
    const Point size(gui.get<int>(GuiSettingsId::RENDER_WIDTH), gui.get<int>(GuiSettingsId::RENDER_HEIGHT));
    vis.camera = Factory::getCamera(gui, size);
    // reset renderer with new params
    /// \todo this needs to be generic
    this->setRenderer(makeAuto<ParticleRenderer>(gui));
}

void Controller::update(const Storage& storage) {
    // fill the combobox with available colorizer
    /// \todo can we do this safely from run thread?
    executeOnMainThread([this, &storage] { //
        window->runStarted();
        window->setColorizerList(this->getColorizerList(storage, false, {}));
    });

    // draw initial positions of particles
    /// \todo generalize stats
    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    this->redraw(storage, stats);

    // set up animation object
    movie = this->createMovie(storage);
}

void Controller::setRunning() {
    status = Status::RUNNING;
}

bool Controller::shouldAbortRun() const {
    return status == Status::STOPPED || status == Status::QUITTING;
}

bool Controller::isQuitting() const {
    return status == Status::QUITTING;
}

Array<SharedPtr<IColorizer>> Controller::getColorizerList(const Storage& storage,
    const bool forMovie,
    const FlatMap<ColorizerId, Palette>& overrides) const {
    // Available colorizers for display and movie are currently hardcoded
    Array<ColorizerId> colorizerIds;
    colorizerIds.push(ColorizerId::COROTATING_VELOCITY);
    if (storage.has(QuantityId::DENSITY)) {
        colorizerIds.push(ColorizerId::DENSITY_PERTURBATION);
    }
    if (storage.has(QuantityId::DAMAGE)) {
        colorizerIds.push(ColorizerId::DAMAGE_ACTIVATION);
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
        colorizerIds.push(ColorizerId::ID);

        if (storage.has(QuantityId::NEIGHBOUR_CNT)) {
            colorizerIds.push(ColorizerId::BOUNDARY);
        }
    }

    Array<QuantityId> quantityColorizerIds{
        QuantityId::PRESSURE,
        QuantityId::ENERGY,
        QuantityId::DEVIATORIC_STRESS,
        QuantityId::DAMAGE,
        QuantityId::VELOCITY_DIVERGENCE,
        QuantityId::DENSITY_VELOCITY_DIVERGENCE,
        QuantityId::VELOCITY_LAPLACIAN,
        QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE,
        QuantityId::FRICTION,
    };
    if (!forMovie) {
        quantityColorizerIds.push(QuantityId::DENSITY);
        quantityColorizerIds.push(QuantityId::MASS);
        quantityColorizerIds.push(QuantityId::AV_ALPHA);
        quantityColorizerIds.push(QuantityId::AV_BALSARA);
        quantityColorizerIds.push(QuantityId::ANGULAR_VELOCITY);
        quantityColorizerIds.push(QuantityId::MOMENT_OF_INERTIA);
        quantityColorizerIds.push(QuantityId::STRENGTH_VELOCITY_GRADIENT);
        quantityColorizerIds.push(QuantityId::STRAIN_RATE_CORRECTION_TENSOR);
        quantityColorizerIds.push(QuantityId::EPS_MIN);
    }

    Array<SharedPtr<IColorizer>> colorizers;
    // add velocity (always present)
    colorizers.push(Factory::getColorizer(gui, ColorizerId::VELOCITY));

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
        colorizers.push(Factory::getColorizer(gui, id, overrides));
    }
    return colorizers;
}

SharedPtr<Bitmap> Controller::getRenderedBitmap() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    RenderParams params;
    params.particles.scale = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
    params.size = Point(gui.get<int>(GuiSettingsId::VIEW_WIDTH), gui.get<int>(GuiSettingsId::VIEW_HEIGHT));
    params.selectedParticle = vis.selectedParticle;

    SharedPtr<Bitmap> bitmap;
    if (!vis.isInitialized()) {
        // not initialized yet, return empty bitmap
        bitmap = makeShared<Bitmap>(wxNullBitmap);
    } else {
        bitmap = vis.renderer->render(*vis.camera, params, *vis.stats);
        ASSERT(bitmap->isOk());
    }
    return bitmap;
}

SharedPtr<ICamera> Controller::getCurrentCamera() const {
    ASSERT(vis.camera != nullptr);
    return vis.camera;
}

Optional<Particle> Controller::getIntersectedParticle(const Point position, const float toleranceEps) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);

    if (!vis.colorizer->isInitialized()) {
        // we have to wait for redraw to get data from storage
        return NOTHING;
    }

    const float radius = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
    const Ray ray = vis.camera->unproject(position);
    const Vector dir = getNormalized(ray.target - ray.origin);

    struct {
        float t = -INFTY;
        Size idx = -1;
        bool wasHitOutside = true;
    } first;

    const Float cutoff = gui.get<Float>(GuiSettingsId::ORTHO_CUTOFF);
    for (Size i = 0; i < vis.positions.size(); ++i) {
        Optional<ProjectedPoint> p = vis.camera->project(vis.positions[i]);
        if (!p) {
            // particle not visible by the camera
            continue;
        }
        if (cutoff != 0._f && abs(dot(vis.camera->getDirection(), vis.positions[i])) > cutoff) {
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
        Optional<Particle> particle = vis.colorizer->getParticle(first.idx);
        if (particle) {
            // add position to the particle data
            particle->addValue(QuantityId::POSITION, vis.positions[first.idx]);
        }
        return particle;
    }
}

void Controller::setColorizer(const SharedPtr<IColorizer>& newColorizer) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    vis.colorizer = newColorizer;
    this->tryRedraw();
}

void Controller::setRenderer(AutoPtr<IRenderer>&& newRenderer) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    vis.renderer = std::move(newRenderer);
    if (status != Status::RUNNING) {
        ASSERT(sph.run);
        SharedPtr<Storage> storage = sph.run->getStorage();
        if (storage) {
            vis.renderer->initialize(*storage, *vis.colorizer, *vis.camera);
        }
        window->Refresh();
    }
}

void Controller::setSelectedParticle(const Optional<Particle>& particle) {
    vis.selectedParticle = particle;

    if (particle) {
        const Color color = vis.colorizer->evalColor(particle->getIndex());
        window->setSelectedParticle(particle.value(), color);
    } else {
        window->deselectParticle();
    }
}

SharedPtr<Movie> Controller::createMovie(const Storage& storage) {
    ASSERT(sph.run);
    RenderParams params;
    params.particles.scale = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
    params.size.x = gui.get<int>(GuiSettingsId::IMAGES_WIDTH);
    params.size.y = gui.get<int>(GuiSettingsId::IMAGES_HEIGHT);

    AutoPtr<IRenderer> renderer;
    Array<SharedPtr<IColorizer>> colorizers;
    switch (gui.get<RendererEnum>(GuiSettingsId::IMAGES_RENDERER)) {
    case RendererEnum::PARTICLE:
        renderer = makeAuto<ParticleRenderer>(gui);
        colorizers = this->getColorizerList(storage, true, {});
        break;
    case RendererEnum::SURFACE:
        renderer = makeAuto<SurfaceRenderer>(gui);
        colorizers = { Factory::getColorizer(gui, ColorizerId::VELOCITY) };
        break;
    default:
        STOP;
    }
    const Point size(gui.get<int>(GuiSettingsId::IMAGES_WIDTH), gui.get<int>(GuiSettingsId::IMAGES_HEIGHT));
    AutoPtr<ICamera> camera = Factory::getCamera(gui, size);
    return makeShared<Movie>(gui, std::move(renderer), std::move(camera), std::move(colorizers), params);
}

void Controller::redraw(const Storage& storage, Statistics& stats) {
    CHECK_FUNCTION(CheckFunction::NON_REENRANT);
    auto functor = [&storage, &stats, this] { //
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);

        // this lock makes sure we don't execute notify_one before getting to wait
        std::unique_lock<std::mutex> lock(vis.mainThreadMutex);
        vis.stats = makeAuto<Statistics>(stats);
        vis.positions = copyable(storage.getValue<Vector>(QuantityId::POSITION));

        // initialize the currently selected colorizer
        ASSERT(vis.isInitialized());
        vis.colorizer->initialize(storage, RefEnum::STRONG);

        // update the renderer with new data
        vis.renderer->initialize(storage, *vis.colorizer, *vis.camera);

        // repaint the window
        window->Refresh();
        vis.mainThreadVar.notify_one();
    };
    if (isMainThread()) {
        functor();
    } else {
        std::unique_lock<std::mutex> lock(vis.mainThreadMutex);
        executeOnMainThread(functor);
        vis.mainThreadVar.wait(lock);
    }
}

void Controller::tryRedraw() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    if (status != Status::RUNNING && vis.timer->isExpired()) {
        // we can safely access the storage
        ASSERT(sph.run);
        SharedPtr<Storage> storage = sph.run->getStorage();
        if (!storage) {
            return;
        }
        vis.colorizer->initialize(*storage, RefEnum::STRONG);
        vis.renderer->initialize(*storage, *vis.colorizer, *vis.camera);
        vis.timer->restart();
        window->Refresh();
    }
}

void Controller::run(const Path& path) {
    sph.thread = std::thread([this, path] {

        try {
            // create storage and set up initial conditions
            sph.run->setUp();
        } catch (std::exception& e) {
            executeOnMainThread([&e] { //
                wxMessageBox(std::string("Invalid run setup: \n") + e.what(), "Fail", wxOK | wxCENTRE);
            });
            return;
        }

        SharedPtr<Storage> storage = sph.run->getStorage();

        // if we want to resume run from state file, load the storage
        if (!path.empty()) {
            BinaryOutput io;
            Statistics stats;
            Outcome result = io.load(path, *storage, stats);
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
        status = Status::STOPPED;
    });
}

NAMESPACE_SPH_END
