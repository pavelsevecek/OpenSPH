#pragma once

/// \file IRun.h
/// \brief Basic interface defining a single run
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/ForwardDecl.h"
#include "objects/containers/List.h"
#include "objects/wrappers/Interval.h"
#include "objects/wrappers/SharedPtr.h"
#include "physics/Integrals.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class IRunCallbacks;
class ILogFile;
class IOutput;
class ITrigger;

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
    AutoPtr<IRunCallbacks> callbacks;

    /// Logging
    SharedPtr<ILogger> logger;

    /// Stores all SPH particles
    SharedPtr<Storage> storage;

    /// Timestepping
    AutoPtr<ITimeStepping> timeStepping;

    /// Solver
    AutoPtr<ISolver> solver;

    /// Triggers
    List<AutoPtr<ITrigger>> triggers;

public:
    IRun();

    ~IRun();

    /// Prepares the run, sets all initial conditions, creates logger, output, ...
    virtual void setUp() = 0;

    /// Starts the run
    void run();

    SharedPtr<Storage> getStorage() const;

protected:
    /// Called after the run, saves all necessary data, logs run statistics, etc. Is called at the end of
    /// \ref run function.
    virtual void tearDown() = 0;

private:
    void setNullToDefaults();

    void tearDownInternal();
};

NAMESPACE_SPH_END
