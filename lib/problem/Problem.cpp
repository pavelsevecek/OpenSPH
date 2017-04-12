#include "problem/Problem.h"
#include "io/LogFile.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "physics/Integrals.h"
#include "solvers/AbstractSolver.h"
#include "system/Callbacks.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "system/Timer.h"
#include "timestepping/TimeStepping.h"

NAMESPACE_SPH_BEGIN

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

Abstract::Problem::Problem() = default;

Abstract::Problem::~Problem() = default;

void Abstract::Problem::run() {
    Size i = 0;
    // fetch parameters of run from settings
    const Float outputInterval = settings.get<Float>(GlobalSettingsIds::RUN_OUTPUT_INTERVAL);
    const Range timeRange = settings.get<Range>(GlobalSettingsIds::RUN_TIME_RANGE);

    // set uninitilized variables
    setNullToDefaults();

    // run main loop
    Float nextOutput = outputInterval;
    logger->write("Running:");
    Timer runTimer;
    EndingCondition condition(settings.get<Float>(GlobalSettingsIds::RUN_WALLCLOCK_TIME),
        settings.get<int>(GlobalSettingsIds::RUN_TIMESTEP_CNT));
    Statistics stats;

    Outcome result = SUCCESS;
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

        for (auto& log : logFiles) {
            log->write(*storage, stats);
        }
        callbacks->onTimeStep(storage, stats);
        if (callbacks->shouldAbortRun()) {
            result = "Aborted by user";
            break;
        }
        i++;
    }
    logger->write("Run ended after ", runTimer.elapsed<TimerUnit::SECOND>(), "s.");
    if (result) {
        callbacks->onRunEnd(storage, stats);
    } else {
        logger->write(result.error());
    }
}

void Abstract::Problem::setNullToDefaults() {
    ASSERT(storage != nullptr);
    ASSERT(solver != nullptr);
    if (!logger) {
        logger = Factory::getLogger(settings);
    }
    if (!output) {
        output = std::make_unique<NullOutput>();
    }
    if (!callbacks) {
        callbacks = std::make_unique<NullCallbacks>();
    }
    if (!timeStepping) {
        timeStepping = Factory::getTimeStepping(settings, storage);
    }
}

NAMESPACE_SPH_END
