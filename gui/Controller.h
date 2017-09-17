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
class IRun;
class IRenderer;
class ICamera;
class IColorizer;


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
        AutoPtr<IRun> run;
    } sph;

    /// Object for saving image snapshots of the simulations
    SharedPtr<Movie> movie;

    struct Vis {
        /// Cached positions of particles for visualization.
        Array<Vector> positions;

        /// Copy of statistics when the colorizer was initialized
        AutoPtr<Statistics> stats;

        /// Rendered used for rendering the view
        AutoPtr<IRenderer> renderer;

        /// Currently selected colorizer
        SharedPtr<IColorizer> colorizer;

        /// Current camera of the view. The object is shared with parent model.
        SharedPtr<ICamera> camera;

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

    /// \addtogroup Run queries

    /// Returns true if the user aborted the run.
    bool shouldAbortRun() const;

    /// Returns true if the application is shutting down.
    bool isQuitting() const;


    /// \addtogroup Display queries

    /// Returns a list of quantities that can be displayed.
    /// \param storage Particle storage containing data for the colorizer
    /// \param forMovie Whether to return list of colorizers for image output or what interactive preview.
    ///                 Some colorizers are skipped when create image files (boundary, ...)
    Array<SharedPtr<IColorizer>> getColorizerList(const Storage& storage, const bool forMovie) const;

    /// Renders a bitmap of current view. Can only be called from main thread.
    SharedPtr<Bitmap> getRenderedBitmap();

    /// Returns the camera currently used for the rendering
    SharedPtr<ICamera> getCurrentCamera() const;

    /// Returns the particle under given image position.
    /// \param position Position in image coordinates, corresponding to the latest rendered image.
    /// \param toleranceEps Relative addition to effective radius of a particle; particles are considered to
    ///                     be under the point of they are closer than (displayedRadius * (1+toleranceEps)).
    Optional<Particle> getIntersectedParticle(const Point position, const float toleranceEps = 1.f);

    /// Returns the settings object.
    GuiSettings& getGuiSettings();


    /// \addtogroup Display setters

    /// \brief Sets a new colorizer to be displayed
    ///
    /// If the run is currently stopped, the colorizer is applied immediately, otherwise it is necessary to
    /// wait for the end of the current timestep before repaiting the particles. Must be called from main
    /// thread.
    /// \param newColorizer Colorizer replacing the current one.
    void setColorizer(const SharedPtr<IColorizer>& newColorizer);

    /// \brief Sets a new renderer used to draw particles.
    ///
    /// If the run is currently stopped, the renderer is replaced immediately, otherwise it is necessary to
    /// wait for the end of the current timestep before repainting the particles. Must be called from main
    /// thread.
    /// \param newRenderer Renderer replacing the current one.
    void setRenderer(AutoPtr<IRenderer>&& newRenderer);

    /// \brief Sets a selected particle or changes the current selection.
    ///
    /// The selection only affects the interactive view; it can be used by the renderer to highlight a
    /// selected particle, and the window can provide information about the selected particle.
    /// \param particle Particle to selected; if NOTHING, the current selection is cleared.
    void setSelectedParticle(const Optional<Particle>& particle);

    /// \brief If possible, redraws the particles with data from storage.
    ///
    /// This can be done when the run is paused or stopped. Otherwise, it is necessary to wait for the next
    /// time step; function does nothing during the run. Needs to be called from main thread.
    void tryRedraw();

    /// \addtogroup Controlling the run

    /// Sets up and starts a new simulation. Must be called before any other run-related functions can be
    /// called. If a simulation is currently running, it waits until the simulation stops and then starts the
    /// new simulation.
    /// \param run New simulation to start
    void start(AutoPtr<IRun>&& run);

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

    /// \brief Redraws the particles.
    ///
    /// Can be called from any thread. Function blocks until the particles are redrawn.
    void redraw(const Storage& storage, Statistics& stats);

    void run();
};

NAMESPACE_SPH_END
