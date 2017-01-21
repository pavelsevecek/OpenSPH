#pragma once

#include "objects/containers/Tuple.h"
#include "objects/finders/AbstractFinder.h"
#include "objects/wrappers/Range.h"
#include "quantities/QuantityIds.h"
#include "quantities/Iterate.h"
#include "solvers/AbstractSolver.h"
#include "solvers/SolverFactory.h"
#include "sph/boundary/Boundary.h"
#include "sph/initial/Distribution.h"
#include "sph/kernel/Kernel.h"
#include "sph/timestepping/TimeStepping.h"
#include "system/Callbacks.h"
#include "system/Logger.h"
#include "system/Output.h"
#include "physics/Integrals.h"
#include <iostream>


NAMESPACE_SPH_BEGIN


/// class DomainEnforce ?
///  initial distribution of particles ~ domain
///  nothing
///  ghost particles
///  hard domain (setting positions without interactions)
///  periodic conditions


class Problem : public Noncopyable {
private:
    int outputEvery;

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
        const std::shared_ptr<Storage> storage = std::make_shared<Storage>())
        : storage(storage) {
        solver = getSolver(settings);
        outputEvery = settings.get<int>(GlobalSettingsIds::RUN_OUTPUT_STEP);
        logger = Factory::getLogger(settings);
    }

    void run() {
        Size i = 0;

        logger->write("Running:");

        Timer runTimer;
        FrequentStats stats;
        for (Float t (timeRange.lower()); timeRange.upper() > t; t += timeStepping->getTimeStep()) {
            if (callbacks) {
                callbacks->onTimeStep(Float((Extended(t) - timeRange.lower()) / timeRange.size()), storage);
                if (callbacks->shouldAbortRun()) {
                    break;
                }
            }

            // Dump output
            if (output && (i % outputEvery == 0)) {
                output->dump(*storage, t);
            }
            i++;

            // Make time step
            timeStepping->step(*solver, stats);

            // Log
            stats.set(FrequentStatsIds::TIME, t);
            stats.set(FrequentStatsIds::INDEX, (int)i);
            FrequentStatsFormat format;
            format.print(*logger, stats);

            /*AvgDeviatoricStress avgds;
            logger->write("ds = ", avgds.get(*storage));*/
        }
        logger->write("Run ended after ", runTimer.elapsed<TimerUnit::SECOND>(), "s.");
    }
};

NAMESPACE_SPH_END
