#pragma once

#include "gui/Settings.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/SharedPtr.h"
#include "quantities/Particle.h"
#include <condition_variable>
#include <thread>

NAMESPACE_SPH_BEGIN

class MainWindow;
class Movie;
class Storage;
class Statistics;
class Bitmap;
class Timer;
class Point;
namespace Abstract {
    class Run;
    class Renderer;
    class Camera;
    class Element;
}


class Controller {
private:
    /// Main frame of the application
    RawPtr<MainWindow> window;

    /// Settings of the GUI application
    GuiSettings gui;

    struct {
        /// Thread running the simulation
        std::thread thread;

        /// SPH simulation
        AutoPtr<Abstract::Run> run;
    } sph;

    /// Object for saving image snapshots of the simulations
    SharedPtr<Movie> movie;

    struct Vis {
        /// Cached positions of particles for visualization.
        Array<Vector> positions;

        /// Copy of statistics when the element was initialized
        AutoPtr<Statistics> stats;

        /// Rendered used for rendering the view
        AutoPtr<Abstract::Renderer> renderer;

        /// Currently selected element
        SharedPtr<Abstract::Element> element;

        /// Current camera of the view. The object is shared with parent model.
        SharedPtr<Abstract::Camera> camera;

        /// Current rotation angle of the view
        /// \todo this is a little hardcoded for ortho camera and noninternal frame, possibly generalize
        Float phi = 0.f;

        /// Currently selected particle.
        Optional<Particle> selectedParticle;

        /// CV for waiting till main thread events are processed
        std::mutex mainThreadMutex;
        std::condition_variable mainThreadVar;

        /// Timer controlling refreshing rate of the view
        AutoPtr<Timer> timer;

        void initialize(const GuiSettings& settings);

        bool isInitialized();
    } vis;

    /// Current status of the code, used for communication between thread
    enum class Status {
        RUNNING,  ///< Simulation in progress
        PAUSED,   ///< Run is paused, can be continued or stopped
        STOPPED,  ///< Run has been stopped by the user
        QUITTING, ///< \ref quit has been called, waiting for threads to finish
    } status = Status::STOPPED;

    /// CV for unpausing run thread
    std::mutex continueMutex;
    std::condition_variable continueVar;

public:
    /// Initialize the controller.
    /// \param gui Parameters of the application; see \ref GuiSettings.
    Controller(const GuiSettings& gui);

    ~Controller();

    /// Called every time step.
    void onTimeStep(const Storage& storage, Statistics& stats);

    /// \section Run queries

    /// Returns true if the user aborted the run.
    bool shouldAbortRun() const;

    /// Returns true if the application is shutting down.
    bool isQuitting() const;


    /// \section Display queries

    /// Returns a list of quantities that can be displayed.
    /// \param storage Particle storage containing data for the element
    /// \param forMovie Whether to return list of elements for image output or what interactive preview. Some
    ///                 elements are skipped when create image files (boundary, ...)
    Array<SharedPtr<Abstract::Element>> getElementList(const Storage& storage, const bool forMovie) const;

    /// Renders a bitmap of current view. Can only be called from main thread.
    SharedPtr<Bitmap> getRenderedBitmap();

    /// Returns the camera currently used for the rendering
    SharedPtr<Abstract::Camera> getCurrentCamera() const;

    /// Returns the particle under given image position.
    /// \param position Position in image coordinates, corresponding to the latest rendered image.
    /// \param toleranceEps Relative addition to effective radius of a particle; particles are considered to
    ///                     be under the point of they are closer than (displayedRadius * (1+toleranceEps)).
    Optional<Particle> getIntersectedParticle(const Point position, const float toleranceEps = 1.f);

    /// Returns the settings object.
    GuiSettings& getGuiSettings();


    /// \section Display setters

    /// Sets a new element to be displayed
    void setElement(const SharedPtr<Abstract::Element>& newElement);

    /// Sets a selected particle or changes the current selection. The selection only affects the interactive
    /// view; it can be used by the renderer to highlight a selected particle, and the window can provide
    /// information about the selected particle.
    /// \param particle Particle to selected; if NOTHING, the current selection is cleared.
    void setSelectedParticle(const Optional<Particle>& particle);


    /// \section Controlling the run

    /// Sets up and starts a new simulation. Must be called before any other run-related functions can be
    /// called. If a simulation is currently running, it waits until the simulation stops and then starts the
    /// new simulation.
    /// \param run New simulation to start
    void start(AutoPtr<Abstract::Run>&& run);

    /// Starts the simulation with current setup. Function does nothing if the simulation is already running.
    /// Can be used to continue paused simulation.
    void restart();

    /// Pause the current simulation. Can only be paused at the beginning of a timestep. The function is not
    /// blocking, it does not wait until the simulation completes the current timestep.
    /// Simulation can be continued using \ref start function or stopped with \ref stop.
    void pause();

    /// Stops the current simulation. If no simulation is running, the function does nothing. Next usage of
    /// \ref start will start the simulation from the beginning.
    /// \param waitForFinish If true, the function will block until the run is finished. Otherwise the
    ///                      function is nonblocking.
    void stop(bool waitForFinish = false);

    /// Closes down the model, clears all allocated resources. Must be called only once.
    void quit();

private:
    SharedPtr<Movie> createMovie(const Storage& storage);

    void redraw(const Storage& storage, Statistics& stats);

    void run();
};

NAMESPACE_SPH_END
