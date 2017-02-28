#include "problem/Problem.h"
#include "physics/Integrals.h"
#include "solvers/AbstractSolver.h"
#include "solvers/SolverFactory.h"
#include "sph/timestepping/TimeStepping.h"
#include "system/Callbacks.h"
#include "system/Factory.h"
#include "system/LogFile.h"
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
    logs.push(std::make_unique<CommonStatsLog>(logger));
}

Problem::~Problem() = default;

struct EndingCondition {
private:
    Float wallclockDuraction;
    Size timestepCnt;

public:
    EndingCondition(const Float wallclockDuraction, const Size timestepCnt)
        : wallclockDuraction(wallclockDuraction)
        , timestepCnt(timestepCnt) {}

    bool operator()(const Timer& timer, const Size timestep) {
        if (wallclockDuraction > 0._f && timer.elapsed<TimerUnit::MILLISECOND>() > wallclockDuraction) {
            return true;
        }
        if (timestepCnt > 0 && timestep >= timestepCnt) {
            return true;
        }
        return false;
    }
};

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
    EndingCondition condition(settings.get<Float>(GlobalSettingsIds::RUN_WALLCLOCK_TIME),
        settings.get<int>(GlobalSettingsIds::RUN_TIMESTEP_CNT));
    Statistics stats;

    for (Float t = timeRange.lower(); t < timeRange.upper() && !condition(runTimer, i);
         t += timeStepping->getTimeStep()) {

        // dump output
        if (output && t >= nextOutput) {
            output->dump(*storage, t);
            nextOutput += outputInterval;
        }

        // make time step
        timeStepping->step(*solver, stats);

        // log
        stats.set(StatisticsIds::TOTAL_TIME, t);
        stats.set(StatisticsIds::INDEX, (int)i);

        for (auto& log : logs) {
            log->write(*storage, stats);
        }
        if (callbacks) {
            callbacks->onTimeStep(storage, stats);
            if (callbacks->shouldAbortRun()) {
                break;
            }
        }

        i++;
    }
    logger->write("Run ended after ", runTimer.elapsed<TimerUnit::SECOND>(), "s.");
}

NAMESPACE_SPH_END
