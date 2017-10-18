#pragma once

#include "io/Logger.h"
#include "objects/wrappers/SharedPtr.h"
#include "physics/Integrals.h"
#include "quantities/QuantityIds.h"
#include "run/Trigger.h"
#include "timestepping/TimeStepCriterion.h"

NAMESPACE_SPH_BEGIN

/// \brief Base class for auxilliary files logging run statistics.
class ILogFile : public PeriodicTrigger {
protected:
    SharedPtr<ILogger> logger;

public:
    /// Constructs the log file.
    ///
    /// This base class actually does not use the logger in any way, it is stored there (and required in the
    /// constructor) because all derived classes are expected to use a logger; this way we can reduce the code
    /// duplication.
    /// \param logger Logger for the written data. Must not be nullptr.
    /// \param period Log period in run time. Must be a positive value or zero; zero period means the log
    ///               message is written on every time step.
    explicit ILogFile(const SharedPtr<ILogger>& logger, const Float period = 0._f)
        : PeriodicTrigger(period)
        , logger(logger) {
        ASSERT(this->logger);
    }

    virtual AutoPtr<ITrigger> action(Storage& storage, Statistics& stats) {
        this->write(storage, stats);
        return nullptr;
    }

protected:
    /// Syntactic suggar, used for const-correctness (loggers should not modify storage nor stats) and
    /// throwing out the return value (loggers do not create more triggers)
    virtual void write(const Storage& storage, const Statistics& stats) = 0;
};


class CommonStatsLog : public ILogFile {
public:
    explicit CommonStatsLog(const SharedPtr<ILogger>& logger)
        : ILogFile(logger, 0._f) {}

    virtual void write(const Storage& UNUSED(storage), const Statistics& stats) {
        logger->write("Output #",
            stats.get<int>(StatisticsId::INDEX),
            "  time = ",
            stats.get<Float>(StatisticsId::RUN_TIME));
        CriterionId id = stats.get<CriterionId>(StatisticsId::TIMESTEP_CRITERION);
        std::stringstream ss;
        if (id == CriterionId::DERIVATIVE) {
            ss << stats.get<QuantityId>(StatisticsId::LIMITING_QUANTITY);
        } else {
            ss << id;
        }
        logger->write(
            " - timestep: dt = ", stats.get<Float>(StatisticsId::TIMESTEP_VALUE), " (set by ", ss.str(), ")");
        logger->write(" - neigbours: ", stats.get<MinMaxMean>(StatisticsId::NEIGHBOUR_COUNT));
        logger->write(" - time spent: ", stats.get<int>(StatisticsId::TIMESTEP_ELAPSED), "ms");
    }
};

class IntegralsLog : public ILogFile {
private:
    TotalEnergy energy;
    TotalMomentum momentum;
    TotalAngularMomentum angularMomentum;

public:
    IntegralsLog(const Path& path, const Size interval)
        : ILogFile(makeAuto<FileLogger>(path), interval) {}

protected:
    virtual void write(const Storage& storage, const Statistics& stats) override {
        const Float time = stats.get<Float>(StatisticsId::RUN_TIME);
        logger->write(time,
            " ",
            momentum.evaluate(storage),
            " ",
            energy.evaluate(storage),
            " ",
            angularMomentum.evaluate(storage));
    }
};

NAMESPACE_SPH_END
