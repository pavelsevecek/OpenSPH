#pragma once

/// \file NBodySolver.h
/// \brief Solver performing N-body simulation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "thread/ThreadLocal.h"
#include "timestepping/ISolver.h"
#include <set>

NAMESPACE_SPH_BEGIN

class ICollisionHandler;
class IGravity;
enum class OverlapEnum;


class CollisionRecord {
private:
    Float overlap = 0._f;

    CollisionRecord(const Size i, const Size j, const Float overlap, const Float time)
        : overlap(overlap)
        , i(i)
        , j(j)
        , collisionTime(time) {}

public:
    /// Indices of the collided particles.
    /// \todo temporarily mutable
    mutable Size i;
    mutable Size j;

    Float collisionTime = INFINITY;

    CollisionRecord() = default;

    // sort by collision time
    bool operator<(const CollisionRecord& other) const {
        return std::make_tuple(-overlap, collisionTime, i, j) <
               std::make_tuple(-other.overlap, other.collisionTime, other.i, other.j);
    }

    /// Returns true if there is some collision or overlap
    explicit operator bool() const {
        return overlap > 0._f || collisionTime < INFINITY;
    }

    static CollisionRecord COLLISION(const Size i, const Size j, const Float time) {
        return CollisionRecord(i, j, 0._f, time);
    }

    static CollisionRecord OVERLAP(const Size i, const Size j, const Float overlap) {
        return CollisionRecord(i, j, overlap, INFINITY);
    }

    bool isOverlap() const {
        return overlap > 0._f;
    }

    friend bool isReal(const CollisionRecord& col) {
        return col.isOverlap() ? isReal(col.overlap) : isReal(col.collisionTime);
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

    // for single-threaded search (should be parallelized in the future)
    Array<NeighbourRecord> neighs;

    // cached array of removed particles, used to avoid invalidating indices during collision handling
    std::set<Size> removed;

    struct {
        AutoPtr<ICollisionHandler> handler;

        AutoPtr<INeighbourFinder> finder;
    } collision;

    struct {
        AutoPtr<ICollisionHandler> handler;

        /// Small value specifying allowed overlap of collided particles, used for numerical stability.
        Float allowedRatio;
    } overlap;

public:
    /// \brief Creates the solver, using the gravity implementation specified by settings.
    explicit NBodySolver(const RunSettings& settings);

    /// \brief Creates the solver by passing the user-defined gravity implementation.
    NBodySolver(const RunSettings& settings, AutoPtr<IGravity>&& gravity);

    ~NBodySolver();

    virtual void integrate(Storage& storage, Statistics& stats) override;

    /// Checks and resolves particle collisions
    virtual void collide(Storage& storage, Statistics& stats, const Float dt) override;

    virtual void create(Storage& storage, IMaterial& material) const override;

private:
    static Float getSearchRadius(ArrayView<const Vector> r, ArrayView<const Vector> v, const Float dt);

    CollisionRecord findClosestCollision(const Size i,
        const Float globalRadius,
        const Interval interval,
        ArrayView<Vector> r,
        ArrayView<Vector> v);

    Optional<Float> checkCollision(const Vector& r1,
        const Vector& v1,
        const Vector& r2,
        const Vector& v2,
        const Interval interval) const;

    void forceMerge(const Size i, Storage& storage, const Float searchRadius);
};

NAMESPACE_SPH_END
