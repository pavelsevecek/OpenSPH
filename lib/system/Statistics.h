#pragma once

/// Statistics gathered and periodically displayed during the run.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "math/Means.h"
#include "objects/ForwardDecl.h"
#include "objects/wrappers/Range.h"
#include "objects/wrappers/Value.h"
#include "objects/wrappers/Variant.h"
#include "quantities/QuantityIds.h"
#include <map>

NAMESPACE_SPH_BEGIN

/// Object holding various statistics about current run. Values are set or accumulated by each component of
/// the running problem (timestepping, solver, ...).
class Statistics {
private:
    enum Types { BOOL, INT, FLOAT, MEANS, VALUE, RANGE };

    using ValueType = Variant<bool, int, Float, Means, Value, Range>;

    struct Entry {
        StatisticsIds id;
        std::string name;
        ValueType value;
    };

    std::map<StatisticsIds, Entry> entries;

public:
    Statistics() = default;

    bool has(const StatisticsIds idx) const {
        return entries.find(idx) != entries.end();
    }

    /// Sets new values of a statistic. If the statistic is not stored in the object, the statistic is created
    /// using default constructor before assigning new value into it.
    template <typename TValue>
    void set(const StatisticsIds idx, TValue&& value) {
        using StoreType = ConvertToSize<TValue>;
        entries[idx].value = StoreType(std::forward<TValue>(value));
    }

    /// Accumulate a value into means of given idx. Value does not have to be stored. If there is no
    /// value of given idx, it is created with default constructor prior to accumulating.
    void accumulate(const StatisticsIds idx, const Float value) {
        ValueType& entry = entries[idx].value;
        if (entry.getTypeIdx() == -1) {
            // not initialized
            entry.template emplace<Means>();
        }
        entry.template get<Means>().accumulate(value);
    }

    /// Returns value of a statistic. The value must be stored in the object and must have type TValue,
    /// checked by assert.
    template <typename TValue>
    TValue get(const StatisticsIds idx) const {
        typename std::map<StatisticsIds, Entry>::const_iterator iter = entries.find(idx);
        ASSERT(iter != entries.end());
        using StoreType = ConvertToSize<TValue>;
        const StoreType& value = iter->second.value.template get<StoreType>();
        return TValue(value);
    }
};

/// List of values that are computed and displayed every timestep
enum class StatisticsIds {
    /// Current number of output, indexed from 0
    INDEX,

    /// Current time of the simulation
    TIME,

    /// Current value of timestep
    TIMESTEP_VALUE,

    /// Key of quantity that currently limits the timestep
    TIMESTEP_CRITERION,

    LIMITING_PARTICLE_IDX,

    LIMITING_VALUE,

    LIMITING_DERIVATIVE,

    LIMITING_QUANTITY,

    /// Total number of particles in the run
    PARTICLE_COUNT,

    /// Number of neighbours (min, max, mean)
    NEIGHBOUR_COUNT,
};


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

NAMESPACE_SPH_END
