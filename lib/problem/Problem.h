#pragma once

#include "objects/ForwardDecl.h"
#include "objects/wrappers/Range.h"
#include <memory>

NAMESPACE_SPH_BEGIN

namespace Abstract {
class Output;
class Callbacks;
}

class Problem : public Noncopyable {
private:
    Float outputInterval;

    /// Logging
    std::unique_ptr<Abstract::Logger> logger;

public:
    /// Data output
    std::unique_ptr<Abstract::Output> output;

    /// GUI callbacks
    std::unique_ptr<Abstract::Callbacks> callbacks;

    /// Timestepping
    std::unique_ptr<Abstract::TimeStepping> timeStepping;


    /// Time range of the simulations
    /// \todo other conditions? For example pressure-limited simulations?
    Range timeRange;

    /// Stores all SPH particles
    std::shared_ptr<Storage> storage;

    /// Implements computations of quantities and their temporal evolution
    std::unique_ptr<Abstract::Solver> solver;


    /// initialize problem by constructing solver
    Problem(const GlobalSettings& settings,
        const std::shared_ptr<Storage> storage = std::make_shared<Storage>());

    ~Problem();

    void run();
};

NAMESPACE_SPH_END
