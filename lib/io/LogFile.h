#pragma once

#include "io/Logger.h"
#include "objects/wrappers/SharedPtr.h"
#include "physics/Integrals.h"
#include "quantities/QuantityIds.h"
#include "system/Statistics.h"
#include "timestepping/TimeStepCriterion.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class LogFile : public Polymorphic {
    private:
        Size counter;
        Size interval;

    protected:
        AutoPtr<Abstract::Logger> logger;

    public:
        /// Constructs the log file.
        /// \param logger Logger for the written data. Must not be nullptr.
        /// \param interval Interval of logs in time steps. Must be a positive value; if interval is 1, the
        ///                 log is written every time step.
        LogFile(AutoPtr<Abstract::Logger>&& logger, const Size interval = 1)
            : counter(0)
            , interval(interval)
            , logger(std::move(logger)) {
            ASSERT(logger);
            ASSERT(interval > 0);
        }

        /// Writes the log, given data in storage and statistics
        void write(const Storage& storage, const Statistics& stats) {
            counter++;
            if (counter == interval) {
                this->writeImpl(storage, stats);
                counter = 0;
            }
        }

    protected:
        virtual void writeImpl(const Storage& storage, const Statistics& stats) = 0;
    };
}

class CommonStatsLog : public Abstract::LogFile {
public:
    CommonStatsLog(AutoPtr<Abstract::Logger>&& logger)
        : Abstract::LogFile(std::move(logger), 1) {}

protected:
    virtual void writeImpl(const Storage& UNUSED(storage), const Statistics& stats) {
        logger->write("Output #",
            stats.get<int>(StatisticsId::INDEX),
            "  time = ",
            stats.get<Float>(StatisticsId::TOTAL_TIME));
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
        logger->write("");
    }
};

class IntegralsLog : public Abstract::LogFile {
private:
    TotalEnergy energy;
    TotalMomentum momentum;
    TotalAngularMomentum angularMomentum;

public:
    IntegralsLog(const std::string& path, const Size interval)
        : Abstract::LogFile(makeAuto<FileLogger>(path), interval) {}

protected:
    virtual void writeImpl(const Storage& storage, const Statistics& stats) {
        const Float time = stats.get<Float>(StatisticsId::TOTAL_TIME);
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
