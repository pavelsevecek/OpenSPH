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

bool RunCallbacksProgressibleAdapter::operator()(const Float progress) const {
    Statistics stats;
    stats.set(StatisticsId::RELATIVE_PROGRESS, progress);
    callbacks.onTimeStep(Storage(), stats);
    return !callbacks.shouldAbortRun();
}

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
    SPH_ASSERT(false, "Invalid configuration, asserts should be only enabled in debug builds");
#endif

    // setup the default scheduler, this can be overriden in \ref setUp if needed
    scheduler = Factory::getScheduler(settings);
}

IRun::~IRun() = default;

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


class NullRunCallbacks : public IRunCallbacks {
public:
    virtual void onSetUp(const Storage&, Statistics&) override {}

    virtual void onTimeStep(const Storage&, Statistics&) override {}

    virtual bool shouldAbortRun() const override {
        return false;
    }
};

class IOutputTime : public Polymorphic {
public:
    virtual Optional<Float> getNextTime() = 0;
};

class LinearOutputTime : public IOutputTime {
private:
    Float interval;
    Float time;

public:
    LinearOutputTime(const RunSettings& settings) {
        time = settings.get<Float>(RunSettingsId::RUN_START_TIME);
        interval = settings.get<Float>(RunSettingsId::RUN_OUTPUT_INTERVAL);
    }

    virtual Optional<Float> getNextTime() override {
        Float result = time;
        time += interval;
        return result;
    }
};

class LogarithmicOutputTime : public IOutputTime {
private:
    Float interval;
    Float time;

public:
    LogarithmicOutputTime(const RunSettings& settings) {
        time = settings.get<Float>(RunSettingsId::RUN_START_TIME);
        interval = settings.get<Float>(RunSettingsId::RUN_OUTPUT_INTERVAL);
    }

    virtual Optional<Float> getNextTime() override {
        Float result = time;
        if (time == 0) {
            time += interval;
        } else {
            time *= 2;
        }
        return result;
    }
};

class CustomOutputTime : public IOutputTime {
private:
    Array<Float> times;

public:
    CustomOutputTime(const RunSettings& settings) {
        const std::string list = settings.get<std::string>(RunSettingsId::RUN_OUTPUT_CUSTOM_TIMES);
        Array<std::string> items = split(list, ',');
        for (const std::string& item : items) {
            try {
                times.push(std::stof(item));
            } catch (const std::invalid_argument&) {
                throw InvalidSetup("Cannot convert '" + item + "' to a number");
            }
        }
        if (!std::is_sorted(times.begin(), times.end())) {
            throw InvalidSetup("Output times must be in ascending order");
        }
    }

    virtual Optional<Float> getNextTime() override {
        if (!times.empty()) {
            Float result = times.front();
            times.remove(0);
            return result;
        } else {
            return NOTHING;
        }
    }
};

AutoPtr<IOutputTime> getOutputTimes(const RunSettings& settings) {
    const OutputSpacing spacing = settings.get<OutputSpacing>(RunSettingsId::RUN_OUTPUT_SPACING);
    switch (spacing) {
    case OutputSpacing::LINEAR:
        return makeAuto<LinearOutputTime>(settings);
    case OutputSpacing::LOGARITHMIC:
        return makeAuto<LogarithmicOutputTime>(settings);
    case OutputSpacing::CUSTOM:
        return makeAuto<CustomOutputTime>(settings);
    default:
        NOT_IMPLEMENTED;
    }
}

Statistics IRun::run(Storage& input) {
    NullRunCallbacks callbacks;
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
    const Interval timeRange(
        settings.get<Float>(RunSettingsId::RUN_START_TIME), settings.get<Float>(RunSettingsId::RUN_END_TIME));
    AutoPtr<IOutputTime> outputTime = getOutputTimes(settings);
    Optional<Float> nextOutput = outputTime->getNextTime();

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
        SPH_ASSERT(progress >= 0._f && progress <= 1._f);
        stats.set(StatisticsId::RELATIVE_PROGRESS, progress);
        stats.set(StatisticsId::INDEX, (int)i);

        // dump output
        if (output && nextOutput && t >= nextOutput.value()) {
            Expected<Path> writtenFile = output->dump(*storage, stats);
            if (!writtenFile) {
                logger->write(writtenFile.error());
            }
            nextOutput = outputTime->getNextTime();
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
            result = makeFailed("Aborted by user");
            break;
        }
        i++;
    }
    logger->write("Run ended after ", runTimer.elapsed(TimerUnit::SECOND), "s.");
    if (!result) {
        logger->write(result.error());
    }
    // clear any user data set during the simulation
    storage->setUserData(nullptr);

    this->tearDownInternal(*storage, stats);

    // move data back to parameter
    input = std::move(*storage);
    return stats;
}


void IRun::setNullToDefaults(SharedPtr<Storage> storage) {
    SPH_ASSERT(storage != nullptr);
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
        logWriter = Factory::getLogWriter(logger, settings);
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

class SimpleRun : public IRun {
public:
    SimpleRun(const RunSettings& settings) {
        this->settings = settings;
    }

protected:
    virtual void setUp(SharedPtr<Storage> UNUSED(storage)) override {}

    virtual void tearDown(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {}
};

Outcome doRun(Storage& storage, const RunSettings& settings) {
    try {
        SimpleRun run(settings);
        NullRunCallbacks callbacks;
        run.run(storage, callbacks);
        return SUCCESS;
    } catch (const InvalidSetup& e) {
        return makeFailed(e.what());
    }
}

NAMESPACE_SPH_END
