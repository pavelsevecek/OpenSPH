#include "io/LogWriter.h"
#include "io/Logger.h"
#include "objects/geometry/Box.h"
#include "quantities/Iterate.h"
#include "quantities/Storage.h"
#include "system/Statistics.h"
#include "system/Timer.h"
#include "timestepping/TimeStepCriterion.h"

NAMESPACE_SPH_BEGIN

ILogWriter::ILogWriter(const SharedPtr<ILogger>& logger, const Float period)
    : PeriodicTrigger(period, 0._f)
    , logger(logger) {
    SPH_ASSERT(this->logger);
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
    logger->write(                                                             " - attractors:  :", storage.getAttractors().size());
    printStat<MinMaxMean>(*logger, stats, StatisticsId::NEIGHBOR_COUNT,        " - neighbors:   ");
    printStat<int>(*logger, stats, StatisticsId::TOTAL_COLLISION_COUNT,        " - collisions:  ");
    printStat<int>(*logger, stats, StatisticsId::BOUNCE_COUNT,                 "    * bounces:  ");
    printStat<int>(*logger, stats, StatisticsId::MERGER_COUNT,                 "    * mergers:  ");
    printStat<int>(*logger, stats, StatisticsId::BREAKUP_COUNT,                "    * breakups: ");
    printStat<int>(*logger, stats, StatisticsId::OVERLAP_COUNT,                " - overlaps:    ");
    printStat<int>(*logger, stats, StatisticsId::AGGREGATE_COUNT,              " - aggregates:  ");
    printStat<int>(*logger, stats, StatisticsId::SOLVER_SUMMATION_ITERATIONS,  " - iteration #: ");
    // clang-format on
}

void VerboseLogWriter::write(const Storage& storage, const Statistics& stats) {
    StandardLogWriter::write(storage, stats);

    Box bbox;
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    for (std::size_t i = 0; i < r.size(); ++i) {
        bbox.extend(r[i]);
    }

    logger->write(" - bounding box: ", bbox);
    logger->write(" - min/max values:");
    iterate<VisitorEnum::FIRST_ORDER>(storage, [&](QuantityId id, const auto& v, const auto& dv) {
        Interval range, drange;
        for (Size i = 0; i < v.size(); ++i) {
            range.extend(Interval(minElement(v[i]), maxElement(v[i])));
            drange.extend(Interval(minElement(dv[i]), maxElement(dv[i])));
        }
        std::string name = lowercase(getMetadata(id).quantityName);
        logger->write("    * ", name, ":  ", range, " (derivative ", drange, ")");
    });
    iterate<VisitorEnum::SECOND_ORDER>(
        storage, [&](QuantityId id, const auto& v, const auto&, const auto& d2v) {
            Interval range, drange;
            for (Size i = 0; i < v.size(); ++i) {
                range.extend(Interval(minElement(v[i]), maxElement(v[i])));
                drange.extend(Interval(minElement(d2v[i]), maxElement(d2v[i])));
            }
            std::string name = lowercase(getMetadata(id).quantityName);
            logger->write("    * ", name, ":  ", range, " (derivative ", drange, ")");
        });
    ArrayView<const Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
    ArrayView<const SymmetricTensor> gradv = storage.getValue<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT);
    Interval divvRange, gradvRange;
    for (Size i = 0; i < divv.size(); ++i) {
        divvRange.extend(divv[i]);
        gradvRange.extend(Interval(minElement(gradv[i]), maxElement(gradv[i])));
    }
    logger->write("    * velocity divergence:  ", divvRange);
    logger->write("    * velocity gradient:    ", gradvRange);

    // clang-format on
}

BriefLogWriter::BriefLogWriter(const SharedPtr<ILogger>& logger, const RunSettings& settings)
    : ILogWriter(logger, 0._f) {
    name = settings.get<std::string>(RunSettingsId::RUN_NAME);
}

void BriefLogWriter::write(const Storage& UNUSED(storage), const Statistics& stats) {
    // Timestep number and current run time
    const int index = stats.get<int>(StatisticsId::INDEX);
    const Float time = stats.get<Float>(StatisticsId::RUN_TIME);
    const Float dt = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
    logger->write(name, " #", index, ", time = ", time, ", step = ", dt);
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
