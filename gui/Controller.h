#pragma once

#include "gui/Settings.h"
#include "io/Path.h"
#include "objects/containers/FlatMap.h"
#include "objects/wrappers/Locking.h"
#include "objects/wrappers/SharedPtr.h"
#include "quantities/Particle.h"
#include "system/Settings.h"
#include <condition_variable>
#include <thread>

class wxBitmap;
class wxWindow;
class wxSizer;

NAMESPACE_SPH_BEGIN

class MainWindow;
class Movie;
class Storage;
class Statistics;
class Timer;
class Point;
class Palette;
class IRun;
class IRenderer;
class ICamera;
class IColorizer;
struct DiagnosticsError;
enum class ColorizerId;

/// \brief Status of the code
enum class RunStatus {
    RUNNING,  ///< Simulation in progress
    PAUSED,   ///< Run is paused, can be continued or stopped
    STOPPED,  ///< Run has been stopped by the user
    QUITTING, ///< \ref quit has been called, waiting for threads to finish
};


class IPluginControls : public Polymorphic {
public:
    /// \brief Creates the windows required by the plugin.
    virtual void create(wxWindow* parent, wxSizer* sizer) = 0;

    /// \brief Called when simulation ends, new simulation is started etc.
    virtual void statusChanges(const RunStatus newStatus) = 0;
};

class Controller {
    friend class GuiCallbacks;

private:
    /// Main frame of the application
    RawPtr<MainWindow> window;

    /// Additional application-specific controls
    AutoPtr<IPluginControls> plugin;

    /// Settings of the GUI application
    GuiSettings gui;

    struct {
        /// Thread running the simulation
        std::thread thread;

        /// SPH simulation
        AutoPtr<IRun> run;

        /// List of callbacks executed on the next timestep (on run thread).
        ///
        /// Callbacks take current run time as an argument. The list is cleared every timestep, only callbacks
        /// added between timesteps are executed.
        using TimeStepCallback = Function<void(const Float runTime, const Float timeStep)>;
        Locking<Array<TimeStepCallback>> onTimeStepCallbacks;
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
        Optional<Size> selectedParticle;

        /// CV for waiting till main thread events are processed
        std::mutex mainThreadMutex;
        std::condition_variable mainThreadVar;

        /// Timer controlling refreshing rate of the view
        AutoPtr<Timer> timer;

        void initialize(const GuiSettings& settings);

        bool isInitialized();
    } vis;

    /// Current status used for communication between thread
    RunStatus status = RunStatus::STOPPED;

    /// CV for unpausing run thread
    std::mutex continueMutex;
    std::condition_variable continueVar;

public:
    /// \brief Initialize the controller.
    ///
    /// \param gui Parameters of the application; see \ref GuiSettings.
    explicit Controller(const GuiSettings& gui, AutoPtr<IPluginControls>&& plugin = nullptr);

    ~Controller();

    /// \todo ugly hack, remove
    void setRunning();

    /// \addtogroup Run queries

    /// \brief Returns true if the user aborted the run.
    bool shouldAbortRun() const;

    /// \brief Returns true if the application is shutting down.
    bool isQuitting() const;


    /// \addtogroup Display queries

    /// Returns a list of quantities that can be displayed.
    /// \param storage Particle storage containing data for the colorizer
    /// \param forMovie Whether to return list of colorizers for image output or for interactive preview.
    ///                 Some colorizers are skipped when create image files (boundary, ...)
    /// \param paletteOverrides Sets palettes for specific colorizers. Ordinarilly, default palettes are used
    ///                         as returned by Factory::getPalette, this map can be used for customization.
    Array<SharedPtr<IColorizer>> getColorizerList(const Storage& storage,
        const bool forMovie,
        const FlatMap<ColorizerId, Palette>& paletteOverrides) const;

    /// \brief Renders a bitmap of current view.
    ///
    /// Can only be called from main thread.
    SharedPtr<wxBitmap> getRenderedBitmap();

    /// \brief Returns the camera currently used for the rendering
    SharedPtr<ICamera> getCurrentCamera() const;

    /// \brief Returns the colorizer currently used for rendering into the window.
    SharedPtr<IColorizer> getCurrentColorizer() const;

    /// \brief Returns the particle under given image position, or NOTHING if such particle exists.
    ///
    /// \param position Position in image coordinates, corresponding to the latest rendered image.
    /// \param toleranceEps Relative addition to effective radius of a particle; particles are considered to
    ///                     be under the point of they are closer than (displayedRadius * (1+toleranceEps)).
    Optional<Size> getIntersectedParticle(const Point position, const float toleranceEps = 1.f);

    Optional<Size> getSelectedParticle() const;

    /// Returns the settings object.
    GuiSettings& getParams();


    /// \addtogroup Display setters

    /// \brief Updates the colorizer list, reset the camera and the renderer, etc.
    ///
    /// This does not have to be called manually, unless the run changes parameters in the middle of the
    /// simulation (for example handoff in composite run). This also creates a new image sequence, so it is
    /// necesary to set up new image path or filename mask prior to calling \ref update, otherwise the
    /// previously generated images will be overwritten.
    void update(const Storage& storage);

    /// \brief Updates the UI parameters of the controller.
    ///
    /// Note that not all parameters can be updated once the controller is constructed.
    void setParams(const GuiSettings& settings);

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
    /// \param particleIdx Particle to selected; if NOTHING, the current selection is cleared.
    void setSelectedParticle(const Optional<Size>& particleIdx);

    /// \brief If possible, redraws the particles with data from storage.
    ///
    /// This can be done when the run is paused or stopped. Otherwise, it is necessary to wait for the next
    /// time step; function does nothing during the run. Needs to be called from main thread.
    void tryRedraw();

    /// \addtogroup Controlling the run

    /// \brief Sets up and starts a new simulation.
    ///
    /// Must be called before any other run-related functions can be called. If a simulation is currently
    /// running, it waits until the simulation stops and then starts the new simulation.
    /// \param run New simulation to start
    void start(AutoPtr<IRun>&& run);

    /// \brief Starts the simulation with current setup.
    ///
    /// Function does nothing if the simulation is already running. Can be used to continue paused simulation.
    void restart();

    /// \brief Pause the current simulation.
    ///
    /// Can only be paused at the beginning of a timestep. The function is not blocking, it does not wait
    /// until the simulation completes the current timestep. Simulation can be continued using \ref start
    /// function or stopped with \ref stop.
    void pause();

    /// \brief Stops the current simulation.
    ///
    /// If no simulation is running, the function does nothing. Next usage of \ref start will start the
    /// simulation from the beginning. \param waitForFinish If true, the function will block until the run is
    /// finished. Otherwise the
    ///                      function is nonblocking.
    void stop(bool waitForFinish = false);

    /// \brief Saves the state of the current run to the disk.
    ///
    /// \param path Path to the file where the run state is saved (as binary data).
    void saveState(const Path& path);

    /// \brief Restarts the run, using given state file as initial conditions.
    ///
    /// \param path Path to the state file.
    void loadState(const Path& path);

    /// Closes down the model, clears all allocated resources. Must be called only once.
    void quit();

private:
    /// \brief Called every time step.
    void onTimeStep(const Storage& storage, Statistics& stats);

    /// \brief Called when a problem is reported by the run.
    void onRunFailure(const DiagnosticsError& error, const Statistics& stats);

    SharedPtr<Movie> createMovie(const Storage& storage);

    /// \brief Redraws the particles.
    ///
    /// Can be called from any thread. Function blocks until the particles are redrawn.
    void redraw(const Storage& storage, Statistics& stats);

    void run(const Path& path = Path());
};

NAMESPACE_SPH_END
