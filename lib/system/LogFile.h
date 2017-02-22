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
            getTimeStepCriterion(statistics.get<AllCriterionIds>(StatisticsIds::TIMESTEP_CRITERION)),
            ")");
        logger->write(" - neigbours: ", statistics.get<Means>(StatisticsIds::NEIGHBOUR_COUNT));
        logger->write("");
    }

private:
    INLINE std::string getTimeStepCriterion(const AllCriterionIds id) const {
        switch ((CriterionIds)id) {
        case CriterionIds::CFL_CONDITION:
            return "CFL condition";
        case CriterionIds::ACCELERATION:
            return "Acceleration";
        case CriterionIds::MAXIMAL_VALUE:
            return "Maximal value";
        case CriterionIds::INITIAL_VALUE:
            return "Default value";
        default:
            return getQuantityName(id);
        }
    }
};

NAMESPACE_SPH_END
