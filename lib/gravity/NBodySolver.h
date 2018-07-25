#pragma once

/// \file NBodySolver.h
/// \brief Solver performing N-body simulation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

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

/// TODO unit tests!
///  perfect bounce, perfect sticking, merging - just check for asserts, it shows enough problems

/// \brief Solver computing gravitational interaction of particles.
///
/// The particles are assumed to be point masses. No hydrodynamics or collisions are considered.
class NBodySolver : public ISolver {
private:
    /// Gravity used by the solver
    AutoPtr<IGravity> gravity;

    IScheduler& scheduler;

    struct ThreadData {
        // neighbours for parallelized queries
        Array<NeighbourRecord> neighs;

        FlatSet<CollisionRecord> collisions;
    };

    ThreadLocal<ThreadData> threadData;

    // for single-threaded search (should be parallelized in the future)
    Array<NeighbourRecord> neighs;

    // cached array of removed particles, used to avoid invalidating indices during collision handling
    FlatSet<Size> removed;

    // holds computed collisions
    FlatSet<CollisionRecord> collisions;

    Array<Float> searchRadii;

    struct {

        AutoPtr<ICollisionHandler> handler;

        AutoPtr<ISymmetricFinder> finder;

    } collision;

    struct {

        AutoPtr<IOverlapHandler> handler;

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
    NBodySolver(IScheduler& scheduler, const RunSettings& settings);

    /// \brief Creates the solver by passing the user-defined gravity implementation.
    NBodySolver(IScheduler& scheduler, const RunSettings& settings, AutoPtr<IGravity>&& gravity);

    ~NBodySolver();

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

NAMESPACE_SPH_END
