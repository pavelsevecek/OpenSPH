#pragma once

#include "objects/ForwardDecl.h"
#include "objects/wrappers/Range.h"
#include "physics/Integrals.h"
#include "system/Settings.h"
#include <memory>

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Output;
    class Callbacks;
    class LogFile;
}

class Problem : public Noncopyable {
protected:
    GlobalSettings settings;

    /// Logging
    std::shared_ptr<Abstract::Logger> logger;


    /// Timestepping
    std::unique_ptr<Abstract::TimeStepping> timeStepping;

    /// \todo other ending conditions beside time?
    /// options:
    /// - wallclock time
    /// - number of timesteps
    /// - min/max/mean/median of given quantity reaches certain value
    /// - ...

public:
    /// Data output
    std::unique_ptr<Abstract::Output> output;

    /// GUI callbacks
    std::unique_ptr<Abstract::Callbacks> callbacks;

    /// Stores all SPH particles
    std::shared_ptr<Storage> storage;

    /// Implements computations of quantities and their temporal evolution
    std::unique_ptr<Abstract::Solver> solver;

    Array<std::unique_ptr<Abstract::LogFile>> logs;


    /// initialize problem by constructing solver
    Problem(const GlobalSettings& settings, const std::shared_ptr<Storage> storage);

    ~Problem();

    void run();

protected:
    /// Prepares the run, sets all initial conditions, creates logger, output, ...
    // virtual void setup() = 0;
};

NAMESPACE_SPH_END
