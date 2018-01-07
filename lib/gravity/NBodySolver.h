#pragma once

/// \file NBodySolver.h
/// \brief Solver performing N-body simulation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gravity/BruteForceGravity.h"
#include "gravity/Collision.h"
#include "gravity/IGravity.h"
#include "io/Logger.h"
#include "objects/finders/BruteForceFinder.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "thread/ThreadLocal.h"
#include "timestepping/ISolver.h"
#include <set>

NAMESPACE_SPH_BEGIN


struct CollisionRecord {
    /// Indices of the collided particles.
    // mutable to allow modifications in std::set - this is valid since we only sort by the collision time
    mutable Size i;
    mutable Size j;

    Float t_coll = INFINITY;

    // sort by collision time
    bool operator<(const CollisionRecord& other) const {
        return t_coll < other.t_coll;
    }

    explicit operator bool() const {
        return t_coll < INFINITY;
    }
};

/// TODO unit tests!
///  perfect bounce, perfect sticking, merging - just check for asserts, it shows enough problems

/// \brief Solver computing gravitational interaction of particles.
///
/// The particles are assumed to be point masses. No hydrodynamics or collisions are considered.
class NBodySolver : public ISolver {
private:
    /// Gravity used by the solver
    AutoPtr<IGravity> gravity;

    /// Thread pool for parallelization
    ThreadPool pool;

    AutoPtr<INeighbourFinder> collisionFinder;

    AutoPtr<ICollisionHandler> collisionHandler;

    /// Small value specifying allowed overlap of collided particles, used for numerical stability.
    Float allowedOverlap;

    // for single-threaded search (should be parallelized in the future)
    Array<NeighbourRecord> neighs;

public:
    /// \brief Creates the solver, using the gravity implementation specified by settings.
    explicit NBodySolver(const RunSettings& settings);

    /// \brief Creates the solver by passing the user-defined gravity implementation.
    NBodySolver(const RunSettings& settings, AutoPtr<IGravity>&& gravity);

    virtual void integrate(Storage& storage, Statistics& stats) override;

    /// Checks and resolves particle collisions
    virtual void collide(Storage& storage, Statistics& stats, const Float dt) override;

    virtual void create(Storage& storage, IMaterial& material) const override;

private:
    static Float getSearchRadius(ArrayView<const Vector> r, ArrayView<const Vector> v, const Float dt);

    CollisionRecord findClosestCollision(const Size i,
        const Float globalRadius,
        const Float dt,
        Array<NeighbourRecord>& neighs,
        ArrayView<Vector> r,
        ArrayView<Vector> v) const;

    Optional<Float> checkCollision(const Vector& r1,
        const Vector& v1,
        const Vector& r2,
        const Vector& v2,
        const Float dt) const;
};

NAMESPACE_SPH_END
