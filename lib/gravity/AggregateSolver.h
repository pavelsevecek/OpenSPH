#pragma once

#include "gravity/NBodySolver.h"
#include "objects/utility/EnumMap.h"

NAMESPACE_SPH_BEGIN

class AggregateHolder;

/// \brief Holds aggregate data stored in the storage and used by the solver.
///
/// Provides functions for querying the state of aggregates.
class IAggregateObserver : public IStorageUserData {
public:
    /// \brief Returns the number of aggregates in the storage.
    ///
    /// Isolated particles do not count as an aggregate.
    virtual Size count() const = 0;
};

enum class AggregateEnum {
    PARTICLES,
    MATERIALS,
    FLAGS,
};

static RegisterEnum<AggregateEnum> sAggregate({
    { AggregateEnum::PARTICLES, "particles", "Aggregate is created for each particles" },
    { AggregateEnum::MATERIALS, "materials", "" },
    { AggregateEnum::FLAGS, "flags", "" },
});

class AggregateSolver : public NBodySolver {
private:
    /// Holds all aggregates in the simulation.
    ///
    /// Shared with storage.
    SharedPtr<AggregateHolder> holder;

public:
    AggregateSolver(IScheduler& scheduler, const RunSettings& settings);

    ~AggregateSolver();

    void createAggregateData(Storage& storage, const AggregateEnum source);

    virtual void integrate(Storage& storage, Statistics& stats) override;

    virtual void collide(Storage& storage, Statistics& stats, const Float dt) override;

    virtual void create(Storage& storage, IMaterial& material) const override;
};


NAMESPACE_SPH_END
