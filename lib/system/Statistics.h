#pragma once

/// \file Statistics.h
/// \brief Statistics gathered and periodically displayed during the run
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/ForwardDecl.h"
#include "math/Means.h"
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

    using ValueType = Variant<bool, int, Float, MinMaxMean, Value, Range>;

    std::map<StatisticsId, ValueType> entries;

public:
    Statistics() = default;

    bool has(const StatisticsId idx) const {
        return entries.find(idx) != entries.end();
    }

    /// Sets new values of a statistic. If the statistic is not stored in the object, the statistic is created
    /// using default constructor before assigning new value into it.
    template <typename TValue>
    void set(const StatisticsId idx, TValue&& value) {
        using StoreType = ConvertToSize<TValue>;
        entries[idx] = StoreType(std::forward<TValue>(value));
    }

    /// Accumulate a value into means of given idx. Value does not have to be stored. If there is no
    /// value of given idx, it is created with default constructor prior to accumulating.
    void accumulate(const StatisticsId idx, const Float value) {
        auto iter = entries.find(idx);
        if (iter != entries.end()) {
            ValueType& entry = iter->second;
            entry.template get<MinMaxMean>().accumulate(value);
        } else {
            MinMaxMean means;
            means.accumulate(value);
            entries.insert({ idx, means });
        }
    }

    /// Returns value of a statistic. The value must be stored in the object and must have type TValue,
    /// checked by assert.
    template <typename TValue>
    TValue get(const StatisticsId idx) const {
        auto iter = entries.find(idx);
        ASSERT(iter != entries.end());
        using StoreType = ConvertToSize<TValue>;
        const StoreType& value = iter->second.template get<StoreType>();
        return TValue(value);
    }
};

/// List of values that are computed and displayed every timestep
enum class StatisticsId {
    /// Current number of output, indexed from 0
    INDEX,

    /// Current time of the simulation in code units. Does not necessarily have to be 0 when run starts.
    TOTAL_TIME,

    /// Progress of the run, always 0 <= progress <= 1, where 0 is the start of the run and 1 is the end of
    /// the run.
    RELATIVE_PROGRESS,

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

    /// Number of iterations used to compute density and smoothing length in summation solver
    SOLVER_SUMMATION_ITERATIONS
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
