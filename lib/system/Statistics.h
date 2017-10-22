#pragma once

/// \file Statistics.h
/// \brief Statistics gathered and periodically displayed during the run
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/ForwardDecl.h"
#include "math/Means.h"
#include "objects/utility/Value.h"
#include "objects/wrappers/Interval.h"
#include "objects/wrappers/Variant.h"
#include "quantities/QuantityIds.h"
#include <map>

NAMESPACE_SPH_BEGIN

/// \brief Object holding various statistics about current run.
///
/// Statistics are stored as key-value pairs, the key being StatisticsId enum defined below. Values are set or
/// accumulated by each component of the running problem (timestepping, solver, ...).
class Statistics {
private:
    enum Types { BOOL, INT, FLOAT, MEANS, VALUE, INTERVAL };

    using ValueType = Variant<bool, int, Float, MinMaxMean, Value, Interval>;

    std::map<StatisticsId, ValueType> entries;

public:
    Statistics() = default;

    /// \brief Checks if the object contains a statistic with given ID
    ///
    /// By default, the object is empty, it contains no data.
    bool has(const StatisticsId idx) const {
        return entries.find(idx) != entries.end();
    }

    /// \brief Sets new values of a statistic.
    ///
    /// If the statistic is not stored in the object, the statistic is created using default constructor
    /// before assigning new value into it.
    template <typename TValue>
    void set(const StatisticsId idx, TValue&& value) {
        using StoreType = ConvertToSize<TValue>;
        entries[idx] = StoreType(std::forward<TValue>(value));
    }

    /// \brief Increments an integer statistic by given amount
    ///
    /// Syntatic suggar, equivalent to get<int>(idx) += amount.
    void increment(const StatisticsId idx, const Size amount) {
        entries[idx].get<int>() += amount;
    }

    /// \brief Accumulate a value into means of given idx.
    ///
    /// Value does not have to be stored. If there is no value of given idx, it is created with default
    /// constructor prior to accumulating.
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

    /// \brief Returns value of a statistic.

    /// The value must be stored in the object and must have type TValue, checked by assert.
    template <typename TValue>
    TValue get(const StatisticsId idx) const {
        auto iter = entries.find(idx);
        ASSERT(iter != entries.end());
        using StoreType = ConvertToSize<TValue>;
        const StoreType& value = iter->second.template get<StoreType>();
        return TValue(value);
    }

    /// \brief Returns value of a statistic, or a given value if the statistic is not stored.
    template <typename TValue>
    TValue getOr(const StatisticsId idx, const TValue& other) const {
        if (this->has(idx)) {
            return this->get<TValue>(idx);
        } else {
            return other;
        }
    }
};

/// List of values that are computed and displayed every timestep
enum class StatisticsId {
    /// Current number of time step, indexed from 0
    INDEX,

    /// Current time of the simulation in code units. Does not necessarily have to be 0 when run starts.
    RUN_TIME,

    /// Progress of the run, always 0 <= progress <= 1, where 0 is the start of the run and 1 is the end of
    /// the run.
    RELATIVE_PROGRESS,

    /// Current value of timestep.
    TIMESTEP_VALUE,

    /// Wallclock time spend on computing last timestep
    TIMESTEP_ELAPSED,

    /// Total number of particles in the run
    PARTICLE_COUNT,

    /// Number of neighbours (min, max, mean)
    NEIGHBOUR_COUNT,

    /// Number of nodes in used gravity tree
    GRAVITY_NODE_COUNT,

    /// Number of tree nodes evaluated by pair-wise interacting
    GRAVITY_NODES_EXACT,

    /// Number of tree nodes evaluated using multipole approximation
    GRAVITY_NODES_APPROX,

    /// Current angular position of the non-inertial frame
    FRAME_ANGLE,

    /// Number of iterations used to compute density and smoothing length in summation solver
    SOLVER_SUMMATION_ITERATIONS,

    /// Criterion that currently limits the timestep.
    TIMESTEP_CRITERION,

    /// Quantity that currently limits the timestep.
    LIMITING_QUANTITY,

    /// Index of particle that currently limits the timestep.
    LIMITING_PARTICLE_IDX,

    /// Quantity value of particle that currently limits the timestep.
    LIMITING_VALUE,

    /// Derivative value of particle that currently limits the timestep.
    LIMITING_DERIVATIVE,
};

NAMESPACE_SPH_END
