#include "problem/Problem.h"
#include "physics/Integrals.h"
#include "solvers/AbstractSolver.h"
#include "solvers/SolverFactory.h"
#include "sph/timestepping/TimeStepping.h"
#include "system/Callbacks.h"
#include "system/Factory.h"
#include "system/Logger.h"
#include "system/Output.h"
#include "system/Statistics.h"
#include "system/Timer.h"

NAMESPACE_SPH_BEGIN

Problem::Problem(const GlobalSettings& settings, const std::shared_ptr<Storage> storage)
    : settings(settings)
    , storage(storage) {
    solver = getSolver(settings);
    logger = Factory::getLogger(settings);
}

Problem::~Problem() = default;

void Problem::run() {
    Size i = 0;
    // fetch parameters of run from settings
    const Float outputInterval = settings.get<Float>(GlobalSettingsIds::RUN_OUTPUT_INTERVAL);
    const Range timeRange = settings.get<Range>(GlobalSettingsIds::RUN_TIME_RANGE);
    // construct timestepping object
    timeStepping = Factory::getTimeStepping(settings, storage);

    // run main loop
    Float nextOutput = outputInterval;
    logger->write("Running:");
    Timer runTimer;
    Statistics stats;
    // ArrayView<Float> energy = storage->getValue<Float>(QuantityIds::ENERGY);
    FileLogger fileLogger("totalEnergy.txt");
    for (Float t = timeRange.lower(); t < timeRange.upper(); t += timeStepping->getTimeStep()) {
        if (callbacks) {
            callbacks->onTimeStep((t - timeRange.lower()) / timeRange.size(), storage);
            if (callbacks->shouldAbortRun()) {
                break;
            }
        }

        // dump output
        if (output && t >= nextOutput) {
            output->dump(*storage, t);
            nextOutput += outputInterval;
        }

        // make time step
        timeStepping->step(*solver, stats);

        // log
        stats.set(StatisticsIds::TIME, t);
        stats.set(StatisticsIds::INDEX, (int)i);
        FrequentStatsFormat format;
        format.print(*logger, stats);

        TotalEnergy en;
        TotalKineticEnergy kinEn;
        TotalInternalEnergy intEn;
        fileLogger.write(t,
            "   ",
            en.evaluate(*storage),
            "   ",
            kinEn.evaluate(*storage),
            "   ",
            intEn.evaluate(*storage));
        i++;
    }
    logger->write("Run ended after ", runTimer.elapsed<TimerUnit::SECOND>(), "s.");
}

NAMESPACE_SPH_END
