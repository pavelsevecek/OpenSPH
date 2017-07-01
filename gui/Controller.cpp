#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/MainLoop.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Element.h"
#include "gui/objects/Movie.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/SurfaceRenderer.h"
#include "gui/windows/MainWindow.h"
#include "gui/windows/OrthoPane.h"
#include "problems/Collision.h"
#include "problems/Rotation.h"
#include "run/Run.h"
#include "system/Timer.h"
#include "thread/CheckFunction.h"
#include <wx/app.h>

NAMESPACE_SPH_BEGIN

Controller::Controller() {
    CHECK_FUNCTION(CheckFunction::ONCE);

    /// \todo there should be some settings of the program, specifying which problem to run and loading
    /// problem-specific GUI settings. However settings like window resolution are problem-independent.

    gui.set(GuiSettingsId::VIEW_FOV, 5.e3_f)
        .set(GuiSettingsId::VIEW_CENTER, Vector(320, 200, 0._f))
        .set(GuiSettingsId::PARTICLE_RADIUS, 0.3_f)
        .set(GuiSettingsId::ORTHO_CUTOFF, 5.e2_f)
        .set(GuiSettingsId::ORTHO_PROJECTION, OrthoEnum::XY)
        .set(GuiSettingsId::IMAGES_SAVE, true)
        .set(GuiSettingsId::IMAGES_TIMESTEP, 0.01_f)
        /// \todo rotation specific
        .set(GuiSettingsId::PALETTE_ENERGY, Range(0.1_f, 10._f))
        .set(GuiSettingsId::PALETTE_PRESSURE, Range(-10._f, 1.e6_f));

    // create objects for drawing particles
    vis.initialize(gui);

    // create main frame of the application
    window = new MainWindow(this, gui);
    window->SetAutoLayout(true);
    window->Show();

    // update the status
    status = Status::RUNNING;

    // create and start the run
    sph.run = makeAuto<AsteroidCollision>(this); // Rotation>(this, 6._f);
    this->run();
}

Controller::~Controller() = default;

void Controller::Vis::initialize(const GuiSettings& gui) {
    renderer = makeAuto<ParticleRenderer>(); // makeAuto<SurfaceRenderer>();
    element = makeAuto<VelocityElement>(gui.get<Range>(GuiSettingsId::PALETTE_VELOCITY));
    timer = makeAuto<Timer>(gui.get<int>(GuiSettingsId::VIEW_MAX_FRAMERATE), TimerFlags::START_EXPIRED);
    camera = Factory::getCamera(gui);
}

bool Controller::Vis::isInitialized() {
    return renderer && stats && element && camera;
}

void Controller::start() {
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

Array<SharedPtr<Abstract::Element>> Controller::getElementList(const Storage& storage,
    const bool forMovie) const {
    // there is no difference between 'physical' quantities we wish to see (density, energy, ...) and
    // other 'internal' quantities (activation strains, yield reduction, ...) in particle storage,
    // we have to provide a list of elements ourselves
    /// \todo should be loaded from a file
    // we only add the element if it is contained in the storage

    Array<ElementId> elementIds{
        ElementId::VELOCITY, ElementId::DENSITY_PERTURBATION,
    };
    if (!forMovie) {
        elementIds.push(ElementId::MOVEMENT_DIRECTION);
        elementIds.push(ElementId::ACCELERATION);
        elementIds.push(ElementId::BOUNDARY);
    }

    Array<QuantityId> quantityElementIds{
        QuantityId::PRESSURE,
        QuantityId::ENERGY,
        QuantityId::DEVIATORIC_STRESS,
        QuantityId::DAMAGE,
        QuantityId::VELOCITY_DIVERGENCE,
    };
    if (!forMovie) {
        quantityElementIds.push(QuantityId::DENSITY);
        quantityElementIds.push(QuantityId::AV_ALPHA);
    }
    Array<SharedPtr<Abstract::Element>> elements;
    for (ElementId id : elementIds) {
        elements.push(Factory::getElement(gui, id));
    }
    for (QuantityId id : quantityElementIds) {
        if (storage.has(id)) {
            elements.push(Factory::getElement(gui, ElementId(id)));
        }
    }
    return elements;
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

SharedPtr<Abstract::Camera> Controller::getCurrentCamera() const {
    ASSERT(vis.camera != nullptr);
    return vis.camera;
}

Optional<Particle> Controller::getIntersectedParticle(const Point position, const float toleranceEps) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);

    if (!vis.element->isInitialized()) {
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
        Optional<Particle> particle = vis.element->getParticle(first.idx);
        if (particle) {
            // add position to the particle data
            particle->addValue(QuantityId::POSITIONS, vis.positions[first.idx]);
        }
        return particle;
    }
}

void Controller::setElement(const SharedPtr<Abstract::Element>& newElement) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    vis.element = newElement;
    if (status != Status::RUNNING) {
        // we can safely access the storage, otherwise the element will be initialized on next timestep
        ASSERT(sph.run);
        SharedPtr<Storage> storage = sph.run->getStorage();
        if (!storage) {
            return;
        }
        vis.element->initialize(*storage, ElementSource::POINTER_TO_STORAGE);
        vis.renderer->initialize(*storage, *vis.element, *vis.camera);
        window->Refresh();
    }
}

void Controller::setSelectedParticle(const Optional<Particle>& particle) {
    vis.selectedParticle = particle;

    if (particle) {
        const Color color = vis.element->eval(particle->getIndex());
        window->setSelectedParticle(particle.value(), color);
    } else {
        window->deselectParticle();
    }
}

SharedPtr<Movie> Controller::createMovie(const Storage& storage) {
    ASSERT(sph.run);
    RenderParams params;
    params.particles.scale = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
    params.size.x = gui.get<int>(GuiSettingsId::RENDER_WIDTH);
    params.size.y = gui.get<int>(GuiSettingsId::RENDER_HEIGHT);

    /// \todo currently hardcoded for ortho render
    AutoPtr<OrthoRenderer> renderer = makeAuto<OrthoRenderer>();
    auto elements = this->getElementList(storage);
    return makeShared<Movie>(gui, std::move(renderer), Factory::getCamera(gui), std::move(elements), params);
}

void Controller::redraw(const Storage& storage, Statistics& stats) {
    CHECK_FUNCTION(CheckFunction::NON_REENRANT);
    auto functor = [&storage, &stats, this] { //
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);

        // this lock makes sure we don't execute notify_one before getting to wait
        std::unique_lock<std::mutex> lock(vis.mainThreadMutex);
        vis.stats = makeAuto<Statistics>(stats);
        vis.positions = copyable(storage.getValue<Vector>(QuantityId::POSITIONS));

        // initialize the currently selected element
        ASSERT(vis.isInitialized());
        vis.element->initialize(storage, ElementSource::CACHE_ARRAYS);

        // update the renderer with new data
        vis.renderer->initialize(storage, *vis.element, *vis.camera);

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

        // fill the combobox with available elements
        /// \todo can we do this safely from run thread?
        executeOnMainThread([this, storage] { //
            window->setElementList(this->getElementList(*storage, false));
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
