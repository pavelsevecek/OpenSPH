#pragma once

#include "gui/Settings.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/SharedPtr.h"
#include <condition_variable>
#include <thread>

NAMESPACE_SPH_BEGIN

class MainWindow;
class Movie;
class Storage;
class Statistics;
class Bitmap;
class Timer;
namespace Abstract {
    class Run;
    class Renderer;
    class Camera;
    class Element;
}


class Controller {
private:
    /// Main frame of the application
    MainWindow* window;

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
        Array<Vector> cached;

        /// Copy of statistics when the element was initialized
        AutoPtr<Statistics> stats;

        /// Rendered used for rendering the view
        AutoPtr<Abstract::Renderer> renderer;

        /// Currently selected element
        SharedPtr<Abstract::Element> element;

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
    /// Initialize the model.
    Controller();

    ~Controller();

    /// Called every time step.
    void onTimeStep(const SharedPtr<Storage>& storage, Statistics& stats);

    /// \section Run queries

    /// Returns true if the user aborted the run.
    bool shouldAbortRun() const;

    /// Returns true if the application is shutting down.
    bool isQuitting() const;

    /// Returns a list of quantities that can be displayed.
    Array<SharedPtr<Abstract::Element>> getElementList(const Storage& storage) const;

    /// Renders a bitmap of current view. Can only be called from main thread.
    Bitmap getRenderedBitmap(Abstract::Camera& camera);

    void setElement(const SharedPtr<Abstract::Element>& newElement);

    /// Returns the settings object.
    GuiSettings& getGuiSettings();

    /// \section Controlling the run

    /// Starts the simulation with current setup. Function does nothing if the simulation is already running.
    /// Can be used to continue paused simulation.
    void start();

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
