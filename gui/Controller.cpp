#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/MainLoop.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Element.h"
#include "gui/objects/Movie.h"
#include "gui/windows/MainWindow.h"
#include "gui/windows/OrthoPane.h"
#include "problems/Collision.h"
#include "run/Run.h"
#include "thread/CheckFunction.h"

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
        .set(GuiSettingsId::IMAGES_TIMESTEP, 0.2_f);

    // create objects for drawing particles
    Point size(gui.get<int>(GuiSettingsId::VIEW_WIDTH), gui.get<int>(GuiSettingsId::VIEW_HEIGHT));
    vis.renderer = std::make_unique<OrthoRenderer>();
    vis.camera = Factory::getCamera(gui);
    vis.element = std::make_unique<VelocityElement>(gui.get<Range>(GuiSettingsId::PALETTE_VELOCITY));

    // create main frame of the application
    window = new MainWindow(this, gui);
    window->SetAutoLayout(true);
    window->Show();

    // update the status
    status = Status::RUNNING;

    // create and start the run
    sph.run = std::make_unique<AsteroidCollision>(this);
    this->run();
}

Controller::~Controller() = default;

bool Controller::Vis::isInitialized() {
    return renderer && camera && element && stats;
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

void Controller::onTimeStep(const std::shared_ptr<Storage>& storage, Statistics& stats) {
    if (status == Status::QUITTING) {
        return;
    }
    // update run progress
    const float progress = stats.get<Float>(StatisticsId::RELATIVE_PROGRESS);
    executeOnMainThread(this->shared_from_this(), [progress](const std::shared_ptr<Controller>& self) { //
        self->window->setProgress(progress);
    });

    // check current time and possibly save images
    ASSERT(movie);
    movie->onTimeStep(storage, stats);

    // update the data for rendering
    this->redraw(*storage, stats);

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

Array<QuantityId> Controller::getElementList(const Storage& storage) const {
    // there is no difference between 'physical' quantities we wish to see (density, energy, ...) and
    // other 'internal' quantities (activation strains, yield reduction, ...) in particle storage,
    // we have to provide a list of elements ourselves
    /// \todo should be loaded from a file
    // we only add the element if it is contained in the storage

    Array<QuantityId> all{     //
        QuantityId::POSITIONS, // this is translated to velocity element, better solution would be appretiated
        QuantityId::PRESSURE,
        QuantityId::ENERGY,
        QuantityId::DENSITY,
        QuantityId::DEVIATORIC_STRESS,
        QuantityId::DAMAGE,
        QuantityId::AV_ALPHA,
        QuantityId::VELOCITY_DIVERGENCE
    };
    Array<QuantityId> available;
    for (QuantityId id : all) {
        if (storage.has(id)) {
            available.push(id);
        }
    }
    return available;
}

Bitmap Controller::getRenderedBitmap() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    RenderParams params;
    params.camera = vis.camera;
    params.particles.scale = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
    params.size = Point(gui.get<int>(GuiSettingsId::VIEW_WIDTH), gui.get<int>(GuiSettingsId::VIEW_HEIGHT));
    if (!vis.isInitialized()) {
        // not initialized yet, return empty bitmap
        return wxNullBitmap;
    } else {
        Bitmap bitmap = vis.renderer->render(vis.cached, *vis.element, params, *vis.stats);
        ASSERT(bitmap.isOk());
        return bitmap;
    }
}

std::shared_ptr<Abstract::Camera> Controller::getCamera() {
    ASSERT(vis.camera);
    return vis.camera;
}

std::shared_ptr<Movie> Controller::createMovie(const Storage& storage) {
    ASSERT(sph.run);
    Array<std::unique_ptr<Abstract::Element>> elements;
    Array<QuantityId> availableIds = this->getElementList(storage);
    for (QuantityId id : availableIds) {
        elements.emplaceBack(Factory::getElement(gui, id));
    }

    RenderParams params;
    params.camera = Factory::getCamera(gui);
    params.particles.scale = gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS);
    params.size.x = gui.get<int>(GuiSettingsId::RENDER_WIDTH);
    params.size.y = gui.get<int>(GuiSettingsId::RENDER_HEIGHT);

    /// \todo currently hardcoded for ortho render
    std::unique_ptr<OrthoRenderer> renderer = std::make_unique<OrthoRenderer>();

    return std::make_shared<Movie>(gui, std::move(renderer), params, std::move(elements));
}

void Controller::redraw(const Storage& storage, Statistics& stats) {
    CHECK_FUNCTION(CheckFunction::NON_REENRANT);
    auto functor = [&storage, &stats, this] { //
        // this lock makes sure we don't execute notify_one before getting to wait
        std::unique_lock<std::mutex> lock(vis.mainThreadMutex);
        vis.stats = std::make_unique<Statistics>(stats);
        vis.cached = copyable(storage.getValue<Vector>(QuantityId::POSITIONS));
        ASSERT(vis.isInitialized());
        vis.element->initialize(storage, ElementSource::CACHE_ARRAYS);
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
        std::shared_ptr<Storage> storage = sph.run->setUp();
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
