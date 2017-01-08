#pragma once

/// Statistics gathered and periodically displayed during the run.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/wrappers/Range.h"
#include "objects/wrappers/Variant.h"
#include "quantities/QuantityKey.h"
#include "system/FloatStats.h"
#include "system/Logger.h"
#include <map>

NAMESPACE_SPH_BEGIN

/// \todo very similar to settings, maybe put to common base
template <typename TEnum>
class Statistics {
private:
    enum Types { BOOL, INT, FLOAT, FLOAT_STATS, RANGE, VECTOR };

    using Value = Variant<bool, int, Float, FloatStats, Range, QuantityKey>;

    struct Entry {
        TEnum id;
        std::string name;
        Value value;
    };

    std::map<TEnum, Entry> entries;

public:
    Statistics() = default;

    bool has(const TEnum idx) { return entries.find(idx) != entries.end(); }

    /// Sets new values of a statistic. If the statistic is not stored in the object, the statistic is created
    /// using default constructor before assigning new value into it.
    template <typename TValue>
    void set(const TEnum idx, TValue&& value) {
        entries[idx].value = std::forward<TValue>(value);
    }

    /// Accumulate a value into float stats of given idx. Value does not have to be stored. If there is no
    /// value of given idx, it is created with default constructor prior to accumulating.
    void accumulate(const TEnum idx, const Float value) {
        Value& entry = entries[idx].value;
        if (entry.getTypeIdx() == -1) {
            // not initialized
            entry.template emplace<FloatStats>();
        }
        entry.template get<FloatStats>().accumulate(value);
    }

    /// Returns value of a statistic. The value must be stored in the object and must have type TValue,
    /// checked by assert.
    template <typename TValue>
    TValue get(const TEnum idx) const {
        typename std::map<TEnum, Entry>::const_iterator iter = entries.find(idx);
        ASSERT(iter != entries.end());
        const TValue& value = iter->second.value.template get<TValue>();
        return value;
    }

    /// Returns an average value of float stats of given idx. Statistic with given idx must be stored in the
    /// object and must be of type FloatStats.
    Extended average(const TEnum idx) const {
        FloatStats stats = get<FloatStats>(idx);
        return stats.average();
    }
};

/// List of values that are computed and displayed every timestep
enum class FrequentStatsIds {
    /// Current number of output, indexed from 0
    INDEX,

    /// Current time of the simulation
    TIME,

    /// Current value of timestep
    TIMESTEP_VALUE,

    /// Key of quantity that currently limits the timestep
    TIMESTEP_CRITERION,

    /// Total number of particles in the run
    PARTICLE_COUNT,

    /// Number of neighbours (min, max, mean)
    NEIGHBOUR_COUNT,
};
using FrequentStats = Statistics<FrequentStatsIds>;

/// List of values computed infrequently, at given times or every X timesteps. Mainly values that takes some
/// time to compute and computing them every timestep would add unnecessary overhead.
enum class SparseStatsIds {
    /// Time of this output
    TIME,

    /// Total momentum of all particles, with a respect to reference frame
    TOTAL_MOMENTUM,

    /// Total momentum of all particles, with a respect to reference frame
    TOTAL_ANGULAR_MOMENTUM,

    /// Total kinetic energy of all particles, with a respect to reference frame
    TOTAL_KINETIC_ENERGY,

    /// Total internal energy of all particles (doesn't depend on reference frame)
    TOTAL_INTERNAL_ENERGY,

    /// Total energy (kinetic + internal) of all particles, with a respect to reference frame
    TOTAL_ENERGY,

    /// Number of components (a.k.a separated bodies).
    COMPONENT_COUNT,
};
using SparseStats = Statistics<SparseStatsIds>;


namespace Abstract {
    template <typename TEnum>
    class StatisticFormat {
    public:
        virtual void print(Abstract::Logger& logger, const Statistics<TEnum>& statistics) const = 0;
    };
}

class FrequentStatsFormat : public Abstract::StatisticFormat<FrequentStatsIds> {
public:
    virtual void print(Abstract::Logger& logger,
        const Statistics<FrequentStatsIds>& statistics) const override {
        logger.write("Output #",
            statistics.get<Float>(FrequentStatsIds::INDEX),
            "time = ",
            statistics.get<Float>(FrequentStatsIds::TIME));
        logger.write(" - timestep: dt = ",
            statistics.get<Float>(FrequentStatsIds::TIMESTEP_VALUE),
            " (set by ",
            getTimeStepCriterion(statistics.get<QuantityKey>(FrequentStatsIds::TIMESTEP_CRITERION)),
            ")");
    }

private:
    std::string getTimeStepCriterion(const QuantityKey key) const {
        switch (key) {
        case QuantityKey::SOUND_SPEED:
            return "CFL condition";
        case QuantityKey::POSITIONS:
            return "Acceleration";
        case QuantityKey::MATERIAL_IDX: // default value, only displayed when adaptive timestep is turned off
            return "Default value";
        default:
            return getQuantityName(key);
        }
    }
};


NAMESPACE_SPH_END
