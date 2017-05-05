#pragma once

#include "io/Logger.h"
#include "objects/wrappers/SharedPtr.h"
#include "quantities/QuantityIds.h"
#include "system/Statistics.h"
#include "timestepping/TimeStepCriterion.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class LogFile : public Polymorphic {
    protected:
        SharedPtr<Abstract::Logger> logger;

    public:
        LogFile(const SharedPtr<Abstract::Logger>& logger)
            : logger(logger) {}

        virtual void write(Storage& storage, const Statistics& stats) = 0;
    };
}

class CommonStatsLog : public Abstract::LogFile {
public:
    CommonStatsLog(const SharedPtr<Abstract::Logger>& logger)
        : Abstract::LogFile(logger) {}

    virtual void write(Storage& UNUSED(storage), const Statistics& stats) {
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
        logger->write(" - neigbours: ", stats.get<Means>(StatisticsId::NEIGHBOUR_COUNT));
        logger->write(" - time spent: ", stats.get<int>(StatisticsId::TIMESTEP_ELAPSED), "ms");
        logger->write("");
    }
};
NAMESPACE_SPH_END
