#pragma once

/// Statistics gathered and periodically displayed during the run.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/wrappers/Range.h"
#include "objects/wrappers/Variant.h"
#include "quantities/QuantityKey.h"
#include "system/FloatStats.h"
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

    template <typename TValue>
    void set(TEnum idx, TValue&& value) {
        entries[idx].value = std::forward<TValue>(value);
    }

    template <typename TValue>
    TValue get(TEnum idx) const {
        typename std::map<TEnum, Entry>::const_iterator iter = entries.find(idx);
        ASSERT(iter != entries.end());
        const TValue& value = iter->second.value.template get<TValue>();
        return value;
    }
};

/// List of values that are computed and displayed every timestep
enum class FrequentStats {
    /// Current time of the simulation
    TIME,

    /// Current value of timestep
    TIMESTEP,

    /// Key of quantity that currently limits the timestep
    TIMESTEP_CRITERION,

    /// Total number of particles in the run
    PARTICLE_COUNT,

    /// Number of neighbours (min, max, mean)
    NEIGHBOUR_COUNT,
};

/// List of values computed infrequently, at given times or every X timesteps. Mainly values that takes some
/// time to compute and computing them every timestep would add unnecessary overhead.
enum class SparseStats {
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


NAMESPACE_SPH_END
