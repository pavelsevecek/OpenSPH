#pragma once

/// Statistics gathered and periodically displayed during the run.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "math/Means.h"
#include "objects/ForwardDecl.h"
#include "objects/wrappers/Range.h"
#include "objects/wrappers/Value.h"
#include "objects/wrappers/Variant.h"
#include "physics/Units.h"
#include "quantities/QuantityIds.h"
#include <map>

NAMESPACE_SPH_BEGIN

/// Object holding various statistics about current run. Values are set or accumulated by each component of
/// the running problem (timestepping, solver, ...).
class Statistics {
private:
    enum Types { BOOL, INT, FLOAT, MEANS, VALUE, UNIT, RANGE };

    using ValueType = Variant<bool, int, Float, Means, Value, Unit, Range>;

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
    TOTAL_TIME,

    /// Current value of timestep
    TIMESTEP_VALUE,

    /// Wallclock time spend on computing last timestep
    TIMESTEP_ELAPSED,

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

NAMESPACE_SPH_END
