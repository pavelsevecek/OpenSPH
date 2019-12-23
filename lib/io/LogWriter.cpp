#include "io/LogWriter.h"
#include "io/Logger.h"
#include "quantities/Storage.h"
#include "system/Statistics.h"
#include "system/Timer.h"
#include "timestepping/TimeStepCriterion.h"

NAMESPACE_SPH_BEGIN

ILogWriter::ILogWriter(const SharedPtr<ILogger>& logger, const Float period)
    : PeriodicTrigger(period, 0._f)
    , logger(logger) {
    ASSERT(this->logger);
}

AutoPtr<ITrigger> ILogWriter::action(Storage& storage, Statistics& stats) {
    this->write(storage, stats);
    return nullptr;
}


template <typename T>
void printStat(ILogger& logger,
    const Statistics& stats,
    const StatisticsId id,
    const std::string& message,
    const std::string& unit = "",
    const std::string& emptyValue = "") {
    if (stats.has(id)) {
        logger.write(message, stats.get<T>(id), unit);
    } else if (!emptyValue.empty()) {
        logger.write(message, emptyValue);
    }
}


StandardLogWriter::StandardLogWriter(const SharedPtr<ILogger>& logger, const RunSettings& settings)
    : ILogWriter(logger, 0._f) {
    name = settings.get<std::string>(RunSettingsId::RUN_NAME);
}

void StandardLogWriter::write(const Storage& storage, const Statistics& stats) {
    // Timestep number and current run time
    const int index = stats.get<int>(StatisticsId::INDEX);
    const Float time = stats.get<Float>(StatisticsId::RUN_TIME);
    const int wallclock = stats.get<int>(StatisticsId::WALLCLOCK_TIME);
    const std::string formattedWallclock = getFormattedTime(wallclock);
    logger->write(name, " #", index, "  time = ", time, "  wallclock time: ", formattedWallclock);

    if (stats.has(StatisticsId::RELATIVE_PROGRESS)) {
        const Float progress = stats.get<Float>(StatisticsId::RELATIVE_PROGRESS);
        logger->write(" - progress:    ", int(progress * 100), "%");
        if (progress > 0.05_f) {
            const std::string formattedEta = getFormattedTime(int64_t(wallclock * (1._f / progress - 1._f)));
            logger->write(" - ETA:         ", formattedEta);
        } else {
            logger->write(" - ETA:         N/A");
        }
    }

    /* needs to be fixed
     * if (stats.has(StatisticsId::ETA)) {
        const int eta = stats.get<int>(StatisticsId::ETA);
        const std::string formattedEta = getFormattedTime(eta);
        logger->write(" - ETA:         ", formattedEta);
    } else {
        logger->write(" - ETA:         ", "N/A");
    }*/

    // Timestepping info
    CriterionId id = stats.get<CriterionId>(StatisticsId::TIMESTEP_CRITERION);
    std::stringstream ss;
    if (id == CriterionId::DERIVATIVE) {
        ss << stats.get<QuantityId>(StatisticsId::LIMITING_QUANTITY);
    } else {
        ss << id;
    }
    const Float dt = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
    logger->write(" - timestep:    ", dt, " (set by ", ss.str(), ")");

    // clang-format off
    printStat<int>(*logger, stats, StatisticsId::TIMESTEP_ELAPSED,             " - time spent:  ", "ms");
    printStat<int>(*logger, stats, StatisticsId::SPH_EVAL_TIME,                "    * SPH evaluation:       ", "ms");
    printStat<int>(*logger, stats, StatisticsId::GRAVITY_EVAL_TIME,            "    * gravity evaluation:   ", "ms");
    printStat<int>(*logger, stats, StatisticsId::COLLISION_EVAL_TIME,          "    * collision evaluation: ", "ms");
    printStat<int>(*logger, stats, StatisticsId::GRAVITY_BUILD_TIME,           "    * tree construction:    ", "ms");
    printStat<int>(*logger, stats, StatisticsId::POSTPROCESS_EVAL_TIME,        "    * visualization:        ", "ms");
    logger->write(                                                             " - particles:   ", storage.getParticleCnt());
    printStat<MinMaxMean>(*logger, stats, StatisticsId::NEIGHBOUR_COUNT,       " - neigbours:   ");
    printStat<int>(*logger, stats, StatisticsId::TOTAL_COLLISION_COUNT,        " - collisions:  ");
    printStat<int>(*logger, stats, StatisticsId::BOUNCE_COUNT,                 "    * bounces:  ");
    printStat<int>(*logger, stats, StatisticsId::MERGER_COUNT,                 "    * mergers:  ");
    printStat<int>(*logger, stats, StatisticsId::BREAKUP_COUNT,                "    * breakups: ");
    printStat<int>(*logger, stats, StatisticsId::OVERLAP_COUNT,                " - overlaps:    ");
    printStat<int>(*logger, stats, StatisticsId::AGGREGATE_COUNT,              " - aggregates:  ");
    printStat<int>(*logger, stats, StatisticsId::SOLVER_SUMMATION_ITERATIONS,  " - iteration #: ");
    // clang-format on
}


IntegralsLogWriter::IntegralsLogWriter(const Path& path, const Size period)
    : IntegralsLogWriter(makeAuto<FileLogger>(path), period) {}

IntegralsLogWriter::IntegralsLogWriter(const SharedPtr<ILogger>& logger, const Size period)
    : ILogWriter(logger, period) {}

void IntegralsLogWriter::write(const Storage& storage, const Statistics& stats) {
    const Float time = stats.get<Float>(StatisticsId::RUN_TIME);
    logger->write(time,
        " ",
        momentum.evaluate(storage),
        " ",
        energy.evaluate(storage),
        " ",
        angularMomentum.evaluate(storage));
}


NullLogWriter::NullLogWriter()
    : ILogWriter(makeShared<NullLogger>(), LARGE) {}

void NullLogWriter::write(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) {}

NAMESPACE_SPH_END
