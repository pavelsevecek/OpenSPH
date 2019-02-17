#pragma once

#include "NBodySolver.h"

NAMESPACE_SPH_BEGIN

class AggregateHolder;

/// \brief Holds aggregate data stored in the storage and used by the solver.
///
/// Provides functions for querying the state of aggregates.
class IAggregateObserver : public IStorageUserData {
public:
    /// \brief Returns ID of the aggregate holding the particle with given index.
    ///
    /// The number is NOT necessarily in range [0, count-1]; the ID is fixed for given aggregate. For given
    /// particle, aggregate ID changes only if the aggregate containing the particle is accumulated into
    /// another aggregate.
    ///
    /// If the particle is isolated, the function returns NOTHING.
    virtual Optional<Size> getAggregateId(const Size particleIdx) const = 0;

    /// \brief Returns the number of aggregates in the storage.
    ///
    /// Isolated particles do not count as an aggregate.
    virtual Size count() const = 0;
};

class AggregateSolver : public NBodySolver {
private:
    /// Holds all aggregates in the simulation.
    ///
    /// Shared with storage.
    SharedPtr<AggregateHolder> holder;

public:
    AggregateSolver(IScheduler& scheduler, const RunSettings& settings);

    ~AggregateSolver();

    virtual void integrate(Storage& storage, Statistics& stats) override;

    virtual void collide(Storage& storage, Statistics& stats, const Float dt) override;

private:
    /// \brief Performs a lazy initialization of the aggregate structures.
    ///
    /// This currently cannot be done in \ref create, as we do not support merging of aggregate holders.
    void createLazy(Storage& storage);
};


NAMESPACE_SPH_END
