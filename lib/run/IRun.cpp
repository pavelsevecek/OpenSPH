#include "run/IRun.h"
#include "io/LogFile.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "physics/Integrals.h"
#include "run/RunCallbacks.h"
#include "run/Trigger.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "system/Timer.h"
#include "timestepping/ISolver.h"
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
        if (wallclockDuraction > 0._f && timer.elapsed(TimerUnit::MILLISECOND) > wallclockDuraction) {
            return true;
        }
        if (timestepCnt > 0 && timestep >= timestepCnt) {
            return true;
        }
        return false;
    }
};

IRun::IRun() {
#ifndef SPH_DEBUG
    ASSERT(false, "Invalid configuration, asserts should be only enabled in debug builds");
#endif
}

IRun::~IRun() = default;

void IRun::run() {
    ASSERT(storage);
    Size i = 0;
    // fetch parameters of run from settings
    const Float outputInterval = settings.get<Float>(RunSettingsId::RUN_OUTPUT_INTERVAL);
    const Interval timeRange = settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE);

    // set uninitilized variables
    setNullToDefaults();

    // run main loop
    Float nextOutput = outputInterval;
    logger->write("Running:");
    Timer runTimer;
    EndingCondition condition(settings.get<Float>(RunSettingsId::RUN_WALLCLOCK_TIME),
        settings.get<int>(RunSettingsId::RUN_TIMESTEP_CNT));
    Statistics stats;

    callbacks->onRunStart(*storage, stats);
    Outcome result = SUCCESS;
    for (Float t = timeRange.lower(); t < timeRange.upper() && !condition(runTimer, i);
         t += timeStepping->getTimeStep()) {

        // save current statistics
        stats.set(StatisticsId::RUN_TIME, t);
        stats.set(StatisticsId::WALLCLOCK_TIME, int(runTimer.elapsed(TimerUnit::MILLISECOND)));
        const Float progress = (t - timeRange.lower()) / timeRange.size();
        ASSERT(progress >= 0._f && progress <= 1._f);
        stats.set(StatisticsId::RELATIVE_PROGRESS, progress);
        stats.set(StatisticsId::INDEX, (int)i);

        // dump output
        if (output && t >= nextOutput) {
            output->dump(*storage, stats);
            nextOutput += outputInterval;
        }

        // make time step
        timeStepping->step(*solver, stats);

        // triggers
        for (auto iter = triggers.begin(); iter != triggers.end();) {
            ITrigger& trig = **iter;
            if (trig.condition(*storage, stats)) {
                AutoPtr<ITrigger> newTrigger = trig.action(*storage, stats);
                if (newTrigger) {
                    triggers.pushBack(std::move(newTrigger));
                }
                if (trig.type() == TriggerEnum::ONE_TIME) {
                    iter = triggers.erase(iter);
                    continue;
                }
            }
            ++iter;
        }

        // callbacks
        callbacks->onTimeStep(*storage, stats);
        if (callbacks->shouldAbortRun()) {
            result = "Aborted by user";
            break;
        }
        i++;
    }
    logger->write("Run ended after ", runTimer.elapsed(TimerUnit::SECOND), "s.");
    if (result) {
        callbacks->onRunEnd(*storage, stats);
    } else {
        logger->write(result.error());
    }
    this->tearDownInternal();
}

SharedPtr<Storage> IRun::getStorage() const {
    return storage;
}

void IRun::setNullToDefaults() {
    ASSERT(storage != nullptr);
    if (!solver) {
        solver = Factory::getSolver(settings);
    }
    if (!logger) {
        logger = Factory::getLogger(settings);
    }
    if (!timeStepping) {
        timeStepping = Factory::getTimeStepping(settings, storage);
    }
    if (!output) {
        output = makeAuto<NullOutput>();
    }
    if (!callbacks) {
        callbacks = makeAuto<NullCallbacks>();
    }
}

void IRun::tearDownInternal() {
    this->tearDown();
    triggers.clear();
    output.reset();
    callbacks.reset();
    logger.reset();
    timeStepping.reset();
    solver.reset();
    // keep storage so that we can access particle data after render ends
}

NAMESPACE_SPH_END
