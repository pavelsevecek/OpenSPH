#pragma once

#include "quantities/QuantityIds.h"
#include "sph/timestepping/TimeStepCriterion.h"
#include "system/Logger.h"
#include "system/Statistics.h"
#include <memory>

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class LogFile : public Polymorphic {
    protected:
        std::shared_ptr<Abstract::Logger> logger;

    public:
        LogFile(const std::shared_ptr<Abstract::Logger>& logger)
            : logger(logger) {}

        virtual void write(Storage& storage, const Statistics& statistics) = 0;
    };
}

/// Shows some basic statistics of the run.
class CommonStatsLog : public Abstract::LogFile {
public:
    CommonStatsLog(const std::shared_ptr<Abstract::Logger>& logger)
        : Abstract::LogFile(logger) {}

    virtual void write(Storage& UNUSED(storage), const Statistics& statistics) {
        logger->write("Output #",
            statistics.get<int>(StatisticsIds::INDEX),
            "  time = ",
            statistics.get<Float>(StatisticsIds::TIME));
        logger->write(" - timestep: dt = ",
            statistics.get<Float>(StatisticsIds::TIMESTEP_VALUE),
            " (set by ",
            statistics.get<AllCriterionIds>(StatisticsIds::TIMESTEP_CRITERION),
            ")");
        logger->write(" - neigbours: ", statistics.get<Means>(StatisticsIds::NEIGHBOUR_COUNT));
        logger->write("");
    }
};

NAMESPACE_SPH_END
