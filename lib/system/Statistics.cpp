#include "system/Statistics.h"
#include "system/Logger.h"

NAMESPACE_SPH_BEGIN

void FrequentStatsFormat::print(Abstract::Logger& logger, const Statistics& statistics) const {
    logger.write("Output #",
        statistics.get<int>(StatisticsIds::INDEX),
        "  time = ",
        statistics.get<Float>(StatisticsIds::TIME));
    logger.write(" - timestep: dt = ",
        statistics.get<Float>(StatisticsIds::TIMESTEP_VALUE),
        " (set by ",
        getTimeStepCriterion(statistics.get<QuantityIds>(StatisticsIds::TIMESTEP_CRITERION)),
        ")");
    logger.write("");
}

std::string FrequentStatsFormat::getTimeStepCriterion(const QuantityIds key) const {
    switch (key) {
    case QuantityIds::SOUND_SPEED:
        return "CFL condition";
    case QuantityIds::POSITIONS:
        return "Acceleration";
    case QuantityIds::MATERIAL_IDX: // default value, only displayed when adaptive timestep is turned off
        return "Default value";
    default:
        return getQuantityName(key);
    }
}

NAMESPACE_SPH_END
