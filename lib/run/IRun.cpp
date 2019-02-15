#include "run/IRun.h"
#include "io/LogWriter.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "physics/Integrals.h"
#include "quantities/IMaterial.h"
#include "run/RunCallbacks.h"
#include "run/Trigger.h"
#include "sph/Diagnostics.h"
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
    scheduler = ThreadPool::getGlobalInstance();
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
                callbacks->onRunFailure(result.error(), stats);
                passed = false;
            }
        }
        if (passed) {
            logger->write(" - no problems detected");
        }
        return nullptr;
    }
};

void IRun::run() {
    ASSERT(storage);
    // fetch parameters of run from settings
    const Float outputInterval = settings.get<Float>(RunSettingsId::RUN_OUTPUT_INTERVAL);
    const Interval timeRange = settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE);

    // set uninitilized variables
    setNullToDefaults();

    // add internal triggers
    /// \todo fix! triggers.pushBack(makeAuto<EtaTrigger>());
    const Float diagnosticsInterval = settings.get<Float>(RunSettingsId::RUN_DIAGNOSTICS_INTERVAL);
    if (!diagnostics.empty()) {
        triggers.pushBack(
            makeAuto<DiagnosticsTrigger>(diagnostics, callbacks.get(), logger, diagnosticsInterval));
    }

    Float nextOutput = timeRange.lower();
    logger->write(
        "Running ", settings.get<std::string>(RunSettingsId::RUN_NAME), " for ", timeRange.size(), " s");
    Timer runTimer;
    EndingCondition condition(settings.get<Float>(RunSettingsId::RUN_WALLCLOCK_TIME),
        settings.get<int>(RunSettingsId::RUN_TIMESTEP_CNT));
    Statistics stats;
    const Float initialDt = settings.get<Float>(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP);
    stats.set(StatisticsId::TIMESTEP_VALUE, initialDt);

    callbacks->onRunStart(*storage, stats);
    Outcome result = SUCCESS;

    // run main loop
    Size i = 0;
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
        callbacks->onTimeStep(*storage, stats);
        if (callbacks->shouldAbortRun()) {
            result = "Aborted by user";
            break;
        }
        i++;
    }
    logger->write("Run ended after ", runTimer.elapsed(TimerUnit::SECOND), "s.");
    if (!result) {
        logger->write(result.error());
    }

    this->tearDownInternal(stats);
}

SharedPtr<Storage> IRun::getStorage() const {
    ASSERT(storage);
    return storage;
}

void IRun::setNullToDefaults() {
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
    if (!callbacks) {
        callbacks = makeAuto<NullCallbacks>();
    }
}

void IRun::tearDownInternal(Statistics& stats) {
    this->tearDown(stats);
    // needs to be called after tearDown, as we signal the storage is not changing anymore
    callbacks->onRunEnd(*storage, stats);

    triggers.clear();
    output.reset();
    callbacks.reset();
    logger.reset();
    logWriter.reset();
    timeStepping.reset();
    solver.reset();
    // keep storage so that we can access particle data after run ends
}

NAMESPACE_SPH_END
