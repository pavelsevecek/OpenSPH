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
    renderer = makeAuto<ParticleRenderer>(); // makeAuto<SurfaceRenderer>();
    colorizer = makeAuto<VelocityColorizer>(gui.get<Interval>(GuiSettingsId::PALETTE_VELOCITY));
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
    if (waitForFinish && sph.thread.joinable()) {
        sph.thread.join();
    }
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

    // pause if we are supposed to
    if (status == Status::PAUSED) {
        std::unique_lock<std::mutex> lock(continueMutex);
        continueVar.wait(lock);
    }
}

GuiSettings& Controller::getGuiSettings() {
    return gui;
}

bool Controller::shouldAbortRun() const {
    return status == Status::STOPPED || status == Status::QUITTING;
}

bool Controller::isQuitting() const {
    return status == Status::QUITTING;
}

Array<SharedPtr<IColorizer>> Controller::getColorizerList(const Storage& storage, const bool forMovie) const {
    // there is no difference between 'physical' quantities we wish to see (density, energy, ...) and
    // other 'internal' quantities (activation strains, yield reduction, ...) in particle storage,
    // we have to provide a list of colorizers ourselves
    /// \todo should be loaded from a file
    // we only add the colorizer if it is contained in the storage

    Array<ColorizerId> colorizerIds{
        ColorizerId::VELOCITY, ColorizerId::DENSITY_PERTURBATION,
    };
    if (!forMovie) {
        colorizerIds.push(ColorizerId::MOVEMENT_DIRECTION);
        colorizerIds.push(ColorizerId::ACCELERATION);
        colorizerIds.push(ColorizerId::BOUNDARY);
    }

    Array<QuantityId> quantityColorizerIds{
        QuantityId::PRESSURE,
        QuantityId::ENERGY,
        QuantityId::DEVIATORIC_STRESS,
        QuantityId::DAMAGE,
        QuantityId::VELOCITY_DIVERGENCE,
    };
    if (!forMovie) {
        quantityColorizerIds.push(QuantityId::DENSITY);
        quantityColorizerIds.push(QuantityId::AV_ALPHA);
    }
    Array<SharedPtr<IColorizer>> colorizers;
    for (ColorizerId id : colorizerIds) {
        colorizers.push(Factory::getColorizer(gui, id));
    }
    for (QuantityId id : quantityColorizerIds) {
        if (storage.has(id)) {
            colorizers.push(Factory::getColorizer(gui, ColorizerId(id)));
        }
    }
    return colorizers;
}

SharedPtr<Bitmap> Controller::getRenderedBitmap() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    RenderParams params;
    params.particles.scale = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
    params.size = Point(gui.get<int>(GuiSettingsId::VIEW_WIDTH), gui.get<int>(GuiSettingsId::VIEW_HEIGHT));
    if (vis.selectedParticle) {
        params.selectedParticle = vis.selectedParticle->getIndex();
    }
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

    for (Size i = 0; i < vis.positions.size(); ++i) {
        Optional<ProjectedPoint> p = vis.camera->project(vis.positions[i]);
        if (!p) {
            // particle not visible by the camera
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
            if (t > first.t || (first.wasHitOutside && !wasHitOutside)) {
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
            particle->addValue(QuantityId::POSITIONS, vis.positions[first.idx]);
        }
        return particle;
    }
}

void Controller::setColorizer(const SharedPtr<IColorizer>& newColorizer) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    vis.colorizer = newColorizer;
    if (status != Status::RUNNING) {
        // we can safely access the storage, otherwise the colorizer will be initialized on next timestep
        ASSERT(sph.run);
        SharedPtr<Storage> storage = sph.run->getStorage();
        if (!storage) {
            return;
        }
        vis.colorizer->initialize(*storage, ColorizerSource::POINTER_TO_STORAGE);
        vis.renderer->initialize(*storage, *vis.colorizer, *vis.camera);
        window->Refresh();
    }
}

void Controller::setSelectedParticle(const Optional<Particle>& particle) {
    vis.selectedParticle = particle;

    if (particle) {
        const Color color = vis.colorizer->eval(particle->getIndex());
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
        renderer = makeAuto<ParticleRenderer>();
        colorizers = this->getColorizerList(storage, true);
        break;
    case RendererEnum::SURFACE:
        renderer = makeAuto<SurfaceRenderer>(gui);
        colorizers = { makeShared<VelocityColorizer>(gui.get<Interval>(GuiSettingsId::PALETTE_VELOCITY)) };
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
        vis.positions = copyable(storage.getValue<Vector>(QuantityId::POSITIONS));

        // rotate camera in case of non-inertial frame
        /// \todo this is a little specific, would be better to somehow get it outside of controller
        if (gui.get<bool>(GuiSettingsId::ORTHO_ROTATE_FRAME) && stats.has(StatisticsId::FRAME_ANGLE)) {
            const Float phi = stats.get<Float>(StatisticsId::FRAME_ANGLE);
            vis.camera->transform(Tensor::rotateZ(phi - vis.phi));
            vis.phi = phi;
        }

        // initialize the currently selected colorizer
        ASSERT(vis.isInitialized());
        vis.colorizer->initialize(storage, ColorizerSource::CACHE_ARRAYS);

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

void Controller::run() {
    sph.thread = std::thread([this] {
        // create storage and set up initial conditions
        sph.run->setUp();
        SharedPtr<Storage> storage = sph.run->getStorage();

        // fill the combobox with available colorizer
        /// \todo can we do this safely from run thread?
        executeOnMainThread([this, storage] { //
            window->setColorizerList(this->getColorizerList(*storage, false));
        });

        // draw initial positions of particles
        /// \todo generalize stats
        Statistics stats;
        stats.set(StatisticsId::TOTAL_TIME, 0._f);
        this->redraw(*storage, stats);

        // set up animation object
        movie = this->createMovie(*storage);

        // run the simulation
        sph.run->run();
    });
}

NAMESPACE_SPH_END
