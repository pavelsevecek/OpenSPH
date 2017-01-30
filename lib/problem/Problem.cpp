#include "problem/Problem.h"
#include "solvers/SolverFactory.h"
#include "system/Factory.h"
#include "system/Logger.h"
#include "system/Timer.h"
#include "system/Statistics.h"
#include "system/Callbacks.h"
#include "system/Output.h"
#include "solvers/AbstractSolver.h"
#include "sph/timestepping/TimeStepping.h"

NAMESPACE_SPH_BEGIN

Problem::Problem(const GlobalSettings& settings,
    const std::shared_ptr<Storage> storage)
    : storage(storage) {
    solver = getSolver(settings);
    outputInterval = settings.get<int>(GlobalSettingsIds::RUN_OUTPUT_INTERVAL);
    logger = Factory::getLogger(settings);
}

Problem::~Problem() = default;

void Problem::run() {
    Size i = 0;
    Float nextOutput = outputInterval;
    logger->write("Running:");

    Timer runTimer;
    Statistics stats;
    for (Float t (timeRange.lower()); timeRange.upper() > t; t += timeStepping->getTimeStep()) {
        if (callbacks) {
            callbacks->onTimeStep((t - timeRange.lower()) / timeRange.size(), storage);
            if (callbacks->shouldAbortRun()) {
                break;
            }
        }

        // Dump output
        if (output && t >= nextOutput) {
            output->dump(*storage, t);
            t += outputInterval;
        }

        // Make time step
        timeStepping->step(*solver, stats);

        // Log
        stats.set(StatisticsIds::TIME, t);
        stats.set(StatisticsIds::INDEX, (int)i);
        FrequentStatsFormat format;
        format.print(*logger, stats);

        i++;
    }
    logger->write("Run ended after ", runTimer.elapsed<TimerUnit::SECOND>(), "s.");
}

NAMESPACE_SPH_END
