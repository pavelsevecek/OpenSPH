#pragma once

/// \file NBodySolver.h
/// \brief Solver performing N-body simulation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/containers/FlatSet.h"
#include "thread/ThreadLocal.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

class ISymmetricFinder;
class ICollisionHandler;
class IOverlapHandler;
class IGravity;
struct CollisionRecord;
class CollisionStats;
enum class CollisionResult;
enum class OverlapEnum;

/// \brief Solver computing gravitational interactions of hard-sphere particles.
class HardSphereSolver : public ISolver {
private:
    /// Gravity used by the solver
    AutoPtr<IGravity> gravity;

    IScheduler& scheduler;

    struct ThreadData {
        /// Neighbours for parallelized queries
        Array<NeighbourRecord> neighs;

        /// Collisions detected by this thread
        Array<CollisionRecord> collisions;
    };

    ThreadLocal<ThreadData> threadData;

    /// List of neighbours, used for single-threaded search
    ///
    /// \note Should be removed in the future.
    Array<NeighbourRecord> neighs;

    /// Cached array of removed particles, used to avoid invalidating indices during collision handling.
    FlatSet<Size> removed;

    /// Holds all detected collisions.
    Array<CollisionRecord> collisions;

    /// Maximum distance to search for impactors, per particle.
    Array<Float> searchRadii;

    struct {

        /// Handler used to resolve particle collisions
        AutoPtr<ICollisionHandler> handler;

        /// Finder for searching the neighbours
        AutoPtr<ISymmetricFinder> finder;

    } collision;

    struct {

        /// Handler used to resolve particle overlaps
        AutoPtr<IOverlapHandler> handler;

        /// Limit ovelap of particle to be classified as "overlap" rather than "collision".
        Float allowedRatio;

    } overlap;

    struct {
        /// Use moment of inertia of individual particles
        bool use;

        /// Maximum rotation of a particle in a single (sub)step.
        Float maxAngle;

    } rigidBody;

    /// Cached views of positions of velocities, so that we don't have to pass it to every function
    ArrayView<Vector> r;
    ArrayView<Vector> v;

public:
    /// \brief Creates the solver, using the gravity implementation specified by settings.
    HardSphereSolver(IScheduler& scheduler, const RunSettings& settings);

    /// \brief Creates the solver by passing the user-defined gravity implementation.
    HardSphereSolver(IScheduler& scheduler, const RunSettings& settings, AutoPtr<IGravity>&& gravity);

    /// \brief Creates the solver by specifying gravity and handlers for collision and overlaps.
    HardSphereSolver(IScheduler& scheduler,
        const RunSettings& settings,
        AutoPtr<IGravity>&& gravity,
        AutoPtr<ICollisionHandler>&& collisionHandler,
        AutoPtr<IOverlapHandler>&& overlapHandler);

    ~HardSphereSolver();

    virtual void integrate(Storage& storage, Statistics& stats) override;

    /// Checks and resolves particle collisions
    virtual void collide(Storage& storage, Statistics& stats, const Float dt) override;

    virtual void create(Storage& storage, IMaterial& material) const override;

private:
    void rotateLocalFrame(Storage& storage, const Float dt);

    enum class SearchEnum {
        /// Finds only particles with lower rank. This option also updates the search radii for each particle,
        /// so that USE_RADII can be used afterwards.
        FIND_LOWER_RANK,

        /// Uses search radius generated with FIND_LOWER_RANK.
        USE_RADII,
    };

    CollisionRecord findClosestCollision(const Size i,
        const SearchEnum opt,
        const Interval interval,
        Array<NeighbourRecord>& neighs);

    /// \brief Checks for collision between particles at positions r1 and r2.
    ///
    /// If the collision happens in time less than given dt, the collision time is returned, otherwise
    /// function returns NULL_OPTIONAL.
    Optional<Float> checkCollision(const Vector& r1,
        const Vector& v1,
        const Vector& r2,
        const Vector& v2,
        const Float dt) const;
};

/// \brief Solver computing gravitational interactions and repulsive forces between particles.
class SoftSphereSolver : public ISolver {
    /// Gravity used by the solver
    AutoPtr<IGravity> gravity;

    IScheduler& scheduler;

    struct ThreadData {
        /// Neighbours for parallelized queries
        Array<NeighbourRecord> neighs;
    };

    ThreadLocal<ThreadData> threadData;

    Float repel;
    Float friction;

public:
    SoftSphereSolver(IScheduler& scheduler, const RunSettings& settings);

    SoftSphereSolver(IScheduler& scheduler, const RunSettings& settings, AutoPtr<IGravity>&& gravity);

    ~SoftSphereSolver();

    virtual void integrate(Storage& storage, Statistics& stats) override;

    virtual void create(Storage& storage, IMaterial& material) const override;
};

NAMESPACE_SPH_END
