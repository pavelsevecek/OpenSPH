#pragma once

/// \file Statistics.h
/// \brief Statistics gathered and periodically displayed during the run
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/ForwardDecl.h"
#include "math/Means.h"
#include "objects/containers/FlatMap.h"
#include "objects/utility/Dynamic.h"
#include "objects/wrappers/Interval.h"
#include "objects/wrappers/Variant.h"
#include "quantities/QuantityIds.h"

NAMESPACE_SPH_BEGIN

/// \brief Object holding various statistics about current run.
///
/// Statistics are stored as key-value pairs, the key being StatisticsId enum defined below. Values are set or
/// accumulated by each component of the running problem (timestepping, solver, ...).
class Statistics {
private:
    enum Types { BOOL, INT, FLOAT, MEANS, VALUE, INTERVAL };

    using ValueType = Variant<bool, int, Float, MinMaxMean, Dynamic, Interval>;

    FlatMap<StatisticsId, ValueType> entries;

public:
    Statistics() = default;

    Statistics(const Statistics& other)
        : entries(other.entries.clone()) {}

    Statistics& operator=(const Statistics& other) {
        entries = other.entries.clone();
        return *this;
    }

    /// \brief Checks if the object contains a statistic with given ID
    ///
    /// By default, the object is empty, it contains no data.
    bool has(const StatisticsId idx) const {
        return entries.contains(idx);
    }

    /// \brief Sets new values of a statistic.
    ///
    /// This overrides any previously stored value.
    template <typename TValue>
    void set(const StatisticsId idx, TValue&& value) {
        using StoreType = ConvertToSize<TValue>;
        entries.insert(idx, StoreType(std::forward<TValue>(value)));
    }

    /// \brief Increments an integer statistic by given amount
    ///
    /// Syntatic suggar, equivalent to set(idx, get<int>(idx) + amount).
    void increment(const StatisticsId idx, const Size amount) {
        if (entries.contains(idx)) {
            entries[idx].template get<int>() += amount;
        } else {
            entries.insert(idx, int(amount));
        }
    }

    /// \brief Accumulate a value into means of given idx.
    ///
    /// Value does not have to be stored. If there is no value of given idx, it is created with default
    /// constructor prior to accumulating.
    void accumulate(const StatisticsId idx, const Float value) {
        Optional<ValueType&> entry = entries.tryGet(idx);
        if (entry) {
            entry->template get<MinMaxMean>().accumulate(value);
        } else {
            MinMaxMean means;
            means.accumulate(value);
            entries.insert(idx, means);
        }
    }

    /// \brief Returns value of a statistic.

    /// The value must be stored in the object and must have type TValue, checked by assert.
    template <typename TValue>
    TValue get(const StatisticsId idx) const {
        Optional<const ValueType&> entry = entries.tryGet(idx);
        ASSERT(entry, int(idx));
        using StoreType = ConvertToSize<TValue>;
        const StoreType& value = entry->template get<StoreType>();
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

    /// Current wallclock duration of the simulation
    WALLCLOCK_TIME,

    /// Progress of the run, always 0 <= progress <= 1, where 0 is the start of the run and 1 is the end of
    /// the run.
    RELATIVE_PROGRESS,

    /// Estimated wallclock time to the end of the simulation
    ETA,

    /// Current value of timestep.
    TIMESTEP_VALUE,

    /// Wallclock time spend on computing last timestep
    TIMESTEP_ELAPSED,

    /// Total number of particles in the run
    PARTICLE_COUNT,

    /// Number of neighbours (min, max, mean)
    NEIGHBOUR_COUNT,

    /// Wallclock duration of evaluation of SPH derivatives
    SPH_EVAL_TIME,

    /// Number of nodes in used gravity tree
    GRAVITY_NODE_COUNT,

    /// Number of tree nodes evaluated by pair-wise interacting
    GRAVITY_NODES_EXACT,

    /// Number of tree nodes evaluated using multipole approximation
    GRAVITY_NODES_APPROX,

    /// Wallclock duration of gravity evaluation
    GRAVITY_EVAL_TIME,

    /// Wallclock duration of collision evaluation
    COLLISION_EVAL_TIME,

    /// Wallclock spent on data dump, particle visualization, etc.
    POSTPROCESS_EVAL_TIME,

    /// Number of collisions in the timestep
    TOTAL_COLLISION_COUNT,

    /// Number of mergers in the timestep
    MERGER_COUNT,

    /// Number of bounce collisions
    BOUNCE_COUNT,

    /// Number of fragmentation collisions
    BREAKUP_COUNT,

    /// Number of particle overlaps detected during collision evaluation
    OVERLAP_COUNT,

    /// Number of aggregates in the simulation (single particles are not counted as aggregates).
    AGGREGATE_COUNT,

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
