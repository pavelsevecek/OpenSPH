#pragma once

/// \file IRun.h
/// \brief Basic interface defining a single run
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/ForwardDecl.h"
#include "objects/containers/List.h"
#include "objects/wrappers/Interval.h"
#include "objects/wrappers/SharedPtr.h"
#include "physics/Integrals.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class IRunCallbacks;
class ILogWriter;
class IScheduler;
class IOutput;
class ITrigger;
class IDiagnostic;

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

    /// GUI callbacks
    SharedPtr<IRunCallbacks> callbacks;

    /// Logging
    SharedPtr<ILogger> logger;

    /// Writes statistics into logger every timestep
    AutoPtr<ILogWriter> logWriter;

    /// Stores all SPH particles
    SharedPtr<Storage> storage;

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

    /// Prepares the run, sets all initial conditions, creates logger, output, ...
    virtual void setUp() = 0;

    /// \brief Starts the run.
    ///
    /// Function assumes that \ref setUp has been called (at least once).
    /// \todo remove the virtual
    virtual void run();

    virtual SharedPtr<Storage> getStorage() const;

protected:
    /// \brief Called after the run
    ///
    /// Used to save the necessary data, log run statistics, etc. Is called at the end of \ref run function.
    /// \param stats Run statistics at the end of the run.
    virtual void tearDown(const Statistics& stats) = 0;

    void setNullToDefaults();

    void tearDownInternal(Statistics& stats);
};

NAMESPACE_SPH_END
