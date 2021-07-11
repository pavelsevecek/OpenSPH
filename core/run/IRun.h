#pragma once

/// \file IRun.h
/// \brief Basic interface defining a single run
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/ForwardDecl.h"
#include "objects/containers/List.h"
#include "objects/wrappers/Interval.h"
#include "objects/wrappers/SharedPtr.h"
#include "physics/Integrals.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class ILogWriter;
class IScheduler;
class IOutput;
class ITrigger;
class IDiagnostic;


/// \brief Callbacks executed by the simulation to provide feedback to the user.
///
/// All functions are called from the same thread that called \ref IRun::run.
class IRunCallbacks : public Polymorphic {
public:
    /// \brief Called right before the run starts, i.e. after initial conditions are set up.
    virtual void onSetUp(const Storage& storage, Statistics& stats) = 0;

    /// \brief Called every timestep.
    ///
    /// This is a blocking call, run is paused until the function returns. This allows to safely access the
    /// storage and run statistics. Note that accessing the storage from different thread during run is
    /// generally unsafe, as the storage can be resized during the run.
    virtual void onTimeStep(const Storage& storage, Statistics& stats) = 0;

    /// \brief Returns whether current run should be aborted or not.
    ///
    /// Can be called any time.
    virtual bool shouldAbortRun() const = 0;
};

/// \brief Defines the interface for a run.
///
/// Each run must implement methods \ref setUp and \ref tearDown, settings up initial conditions for the run
/// and closing down the run, respectively.
///
/// Implementation can set up all member variables to any given value. If any variable is left uninitialized,
/// it will be initialized to a default value as specified by run settings. Only particle storage MUST be
/// initialized by the \ref setUp function. User must call \ref setUp before calling \ref run. After run ends,
/// function \ref setUp must be called again before another run can be started.
///
/// \attention Implementation of \ref setUp function must either create a new storage or clear any previous
/// quantities stored in it. Other member variables do not have to be initialized nor cleared.
///
/// Run is started up using \ref run member function. The function is blocking and ends when run is finished.
/// The function can be called from any thread, but cannot be executed from multiple threads simultaneously.
/// The flow of the run can be controlled from different thread using provided implementation of Callbacks.
class IRun : public Polymorphic {
protected:
    RunSettings settings;

    /// Data output
    AutoPtr<IOutput> output;

    /// Logging
    SharedPtr<ILogger> logger;

    /// Writes statistics into logger every timestep
    AutoPtr<ILogWriter> logWriter;

    /// Scheduler used for parallelization.
    SharedPtr<IScheduler> scheduler;

    /// Timestepping
    AutoPtr<ITimeStepping> timeStepping;

    /// Solver
    AutoPtr<ISolver> solver;

    /// Triggers
    List<AutoPtr<ITrigger>> triggers;

    /// Diagnostics
    Array<AutoPtr<IDiagnostic>> diagnostics;

public:
    IRun();

    ~IRun();

    /// \brief Runs the simulation.
    ///
    /// The provided storage can either be filled with initial conditions specified by the \ref IRun
    /// implementation, or it can use particles already stored in the storage. The function is blocking and
    /// returns after the simulation ends. The storage then contains the particle state at the end of the
    /// simulation.
    Statistics run(Storage& storage);

    /// \copydoc run
    ///
    /// \param callbacks Interface used to obtain particle data during the simulation, etc.
    Statistics run(Storage& storage, IRunCallbacks& callbacks);

protected:
    /// \brief Prepares the run, creates logger, output, ...
    virtual void setUp(SharedPtr<Storage> storage) = 0;

    /// \brief Called after the run
    ///
    /// Used to save the necessary data, log run statistics, etc. Is called at the end of \ref run function.
    /// \param stats Run statistics at the end of the run.
    virtual void tearDown(const Storage& storage, const Statistics& stats) = 0;

    void setNullToDefaults(SharedPtr<Storage> storage);

    void tearDownInternal(const Storage& storage, const Statistics& stats);
};

/// \brief Runs a simulation using provided storage as initial conditions.
///
/// This function is a simple alternative to implementing the IRun interface. By providing the initial
/// conditions and settings that specify the solver, timestepping, end time, etc., functions run the
/// simulation and updates the \ref Storage object with computed values of quantities.
/// \param storage Input/output storage containing initial conditions
/// \param settings Parameters of the simulation
/// \return SUCCESS if computed successfully, otherwise an error message.
Outcome doRun(Storage& storage, const RunSettings& settings);

NAMESPACE_SPH_END
