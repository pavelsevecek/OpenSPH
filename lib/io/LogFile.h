#pragma once

#include "io/Logger.h"
#include "quantities/QuantityIds.h"
#include "system/Statistics.h"
#include "timestepping/TimeStepCriterion.h"
#include <memory>

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class LogFile : public Polymorphic {
    protected:
        std::shared_ptr<Abstract::Logger> logger;

    public:
        LogFile(const std::shared_ptr<Abstract::Logger>& logger)
            : logger(logger) {}

        virtual void write(Storage& storage, const Statistics& stats) = 0;
    };
}

/// Shows some basic statistics of the run.
class CommonStatsLog : public Abstract::LogFile {
public:
    CommonStatsLog(const std::shared_ptr<Abstract::Logger>& logger)
        : Abstract::LogFile(logger) {}

    virtual void write(Storage& UNUSED(storage), const Statistics& stats) {
        logger->write("Output #",
            stats.get<int>(StatisticsIds::INDEX),
            "  time = ",
            stats.get<Float>(StatisticsIds::TOTAL_TIME));
        logger->write(" - timestep: dt = ",
            stats.get<Float>(StatisticsIds::TIMESTEP_VALUE),
            " (set by ",
            stats.get<AllCriterionIds>(StatisticsIds::TIMESTEP_CRITERION),
            ")");
        logger->write(" - neigbours: ", stats.get<Means>(StatisticsIds::NEIGHBOUR_COUNT));
        logger->write(" - time spent: ", stats.get<int>(StatisticsIds::TIMESTEP_ELAPSED), "ms");
        logger->write("");
    }
};

NAMESPACE_SPH_END
