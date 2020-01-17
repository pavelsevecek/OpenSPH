#include "run/IRun.h"
#include "io/LogWriter.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "physics/Integrals.h"
#include "quantities/IMaterial.h"
#include "run/Trigger.h"
#include "sph/Diagnostics.h"
#include "sph/boundary/Boundary.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "system/Timer.h"
#include "thread/Pool.h"
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

    // setup the default scheduler, this can be overriden in \ref setUp if needed
    scheduler = Factory::getScheduler(settings);
}

IRun::~IRun() = default;

/// \todo currently updates only once every 100 steps
class EtaTrigger : public ITrigger {
private:
    Size stepCounter = 0;

    int lastElapsed = 0;
    Float lastProgress = 0._f;

    Float speed = 0._f;

    static constexpr Size RECOMPUTE_PERIOD = 100;

public:
    virtual TriggerEnum type() const override {
        return TriggerEnum::REPEATING;
    }

    virtual bool condition(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {
        return true;
    }

    virtual AutoPtr<ITrigger> action(Storage& UNUSED(storage), Statistics& stats) override {
        const Float progress = stats.get<Float>(StatisticsId::RELATIVE_PROGRESS);

        if (stepCounter++ == RECOMPUTE_PERIOD) {
            const int elapsed = stats.get<int>(StatisticsId::WALLCLOCK_TIME);
            speed = (elapsed - lastElapsed) / (progress - lastProgress);

            lastElapsed = elapsed;
            lastProgress = progress;
            stepCounter = 0;
        }

        if (speed > 0._f) {
            const Float eta = speed * (1._f - progress);
            stats.set(StatisticsId::ETA, eta);
        }
        return nullptr;
    }
};

class DiagnosticsTrigger : public PeriodicTrigger {
private:
    ArrayView<const AutoPtr<IDiagnostic>> diagnostics;

    RawPtr<IRunCallbacks> callbacks;
    SharedPtr<ILogger> logger;

public:
    DiagnosticsTrigger(ArrayView<const AutoPtr<IDiagnostic>> diagnostics,
        RawPtr<IRunCallbacks> callbacks,
        SharedPtr<ILogger> logger,
        const Float period)
        : PeriodicTrigger(period, 0._f)
        , diagnostics(diagnostics)
        , callbacks(callbacks)
        , logger(logger) {}

    virtual AutoPtr<ITrigger> action(Storage& storage, Statistics& stats) {
        logger->write("Running simulation diagnostics");
        bool passed = true;
        for (auto& diag : diagnostics) {
            const DiagnosticsReport result = diag->check(storage, stats);
            if (!result) {
                logger->write(result.error().description);
                passed = false;
            }
        }
        if (passed) {
            logger->write(" - no problems detected");
        }
        return nullptr;
    }
};


class NullCallbacks : public IRunCallbacks {
public:
    virtual void onSetUp(const Storage&, Statistics&) override {}

    virtual void onTimeStep(const Storage&, Statistics&) override {}

    virtual bool shouldAbortRun() const override {
        return false;
    }
};

Statistics IRun::run(Storage& input) {
    NullCallbacks callbacks;
    return this->run(input, callbacks);
}

Statistics IRun::run(Storage& input, IRunCallbacks& callbacks) {
    // setup verbose logging (before setUp to log IC's as well)
    if (settings.get<bool>(RunSettingsId::RUN_VERBOSE_ENABLE)) {
        const Path file(settings.get<std::string>(RunSettingsId::RUN_VERBOSE_NAME));
        const Path outputPath(settings.get<std::string>(RunSettingsId::RUN_OUTPUT_PATH));
        setVerboseLogger(makeAuto<FileLogger>(outputPath / file, FileLogger::Options::ADD_TIMESTAMP));
    } else {
        setVerboseLogger(nullptr);
    }

    // move the data to shared storage, needed for Timestepping
    SharedPtr<Storage> storage = makeShared<Storage>(std::move(input));

    // make initial conditions
    this->setUp(storage);

    // set uninitilized variables
    setNullToDefaults(storage);

    // fetch parameters of run from settings
    const Float outputInterval = settings.get<Float>(RunSettingsId::RUN_OUTPUT_INTERVAL);
    const Interval timeRange(
        settings.get<Float>(RunSettingsId::RUN_START_TIME), settings.get<Float>(RunSettingsId::RUN_END_TIME));

    Float nextOutput = timeRange.lower();
    logger->write(
        "Running ", settings.get<std::string>(RunSettingsId::RUN_NAME), " for ", timeRange.size(), " s");
    Timer runTimer;
    EndingCondition condition(settings.get<Float>(RunSettingsId::RUN_WALLCLOCK_TIME),
        settings.get<int>(RunSettingsId::RUN_TIMESTEP_CNT));
    const Float initialDt = settings.get<Float>(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP);

    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, timeRange.lower());
    stats.set(StatisticsId::TIMESTEP_VALUE, initialDt);

    callbacks.onSetUp(*storage, stats);
    Outcome result = SUCCESS;

    // run main loop
    Size i = 0;
    for (Float t = timeRange.lower(); t < timeRange.upper() && !condition(runTimer, i);
         t += timeStepping->getTimeStep()) {
        // save current statistics
        stats.set(StatisticsId::RUN_TIME, t);
        stats.set(StatisticsId::WALLCLOCK_TIME, int(runTimer.elapsed(TimerUnit::MILLISECOND)));
        const Float progress = t / timeRange.upper();
        ASSERT(progress >= 0._f && progress <= 1._f);
        stats.set(StatisticsId::RELATIVE_PROGRESS, progress);
        stats.set(StatisticsId::INDEX, (int)i);

        // dump output
        if (output && t >= nextOutput) {
            Expected<Path> result = output->dump(*storage, stats);
            if (!result) {
                logger->write(result.error());
            }
            nextOutput += outputInterval;
        }

        // make time step
        timeStepping->step(*scheduler, *solver, stats);

        // log stats
        logWriter->write(*storage, stats);

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
        callbacks.onTimeStep(*storage, stats);
        if (callbacks.shouldAbortRun()) {
            result = "Aborted by user";
            break;
        }
        i++;
    }
    logger->write("Run ended after ", runTimer.elapsed(TimerUnit::SECOND), "s.");
    if (!result) {
        logger->write(result.error());
    }

    this->tearDownInternal(*storage, stats);

    // move data back to parameter
    input = std::move(*storage);
    return stats;
}


void IRun::setNullToDefaults(SharedPtr<Storage> storage) {
    ASSERT(storage != nullptr);
    if (!scheduler) {
        // default to global thread pool
        scheduler = ThreadPool::getGlobalInstance();
    }

    if (!solver) {
        solver = Factory::getSolver(*scheduler, settings);
        for (Size i = 0; i < storage->getMaterialCnt(); ++i) {
            solver->create(*storage, storage->getMaterial(i));
        }
    }
    if (!logger) {
        logger = Factory::getLogger(settings);
    }
    if (!logWriter) {
        logWriter = makeAuto<StandardLogWriter>(logger, settings);
    }
    if (!timeStepping) {
        timeStepping = Factory::getTimeStepping(settings, storage);
    }
    if (!output) {
        output = Factory::getOutput(settings);
    }
}

void IRun::tearDownInternal(const Storage& storage, const Statistics& stats) {
    this->tearDown(storage, stats);

    triggers.clear();
    output.reset();
    logger.reset();
    logWriter.reset();
    timeStepping.reset();
    solver.reset();
    // keep storage so that we can access particle data after run ends
}

NAMESPACE_SPH_END
