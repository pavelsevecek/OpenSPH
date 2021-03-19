#pragma once

/// \file Collision.h
/// \brief Collision handling
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/containers/FlatSet.h"
#include "physics/Functions.h"
#include "quantities/Storage.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

enum class CollisionResult {
    NONE,          ///< No collision took place
    BOUNCE,        ///< Bounce/scatter collision, no merging and no fragmentation
    FRAGMENTATION, ///< Target was disrupted, creating largest remnant and fragments
    MERGER,        ///< Particles merged together
    EVAPORATION,   ///< No asteroids survived the collision
};

/// \brief Abstraction of collision outcome.
///
/// Collision can arbitrarily change the number of particles in the storage. It can one or both colliding
/// particles ("merger" or "evaporation"), or it can even add more particles into the storage
/// ("fragmentation"). It is necessary to update all pointers and array views if collision handler returns
/// true, or keep pointers to the arrays (at a cost of double indirection).
class ICollisionHandler : public Polymorphic {
public:
    virtual void initialize(Storage& storage) = 0;

    /// \brief Computes the outcome of collision between i-th and j-th particle
    ///
    /// It is guaranteed that this function is called after \ref initialize has been called (at least once)
    /// and that the storage object passed to \ref initialize is valid, so it is allowed (and recommended) to
    /// storage a pointer to the storage.
    /// \param i,j Indices of particles in the storage.
    /// \param toRemove Indices of particles to be removed from the storage. May already contain some indices,
    ///                 collision handler should only add new indices and it shall not clear the storage.
    /// \return True if the collision took place, false to reject the collision.
    ///
    /// \todo Needs to be generatelized for fragmentation handlers. Currently the function CANNOT change the
    /// number of particles as it would invalidate arrayviews and we would lost the track of i-th and j-th
    /// particle (which we need for decreasing movement time).
    virtual CollisionResult collide(const Size i, const Size j, FlatSet<Size>& toRemove) = 0;
};

/// \brief Handles overlaps of particles.
///
/// Interface similar to \ref ICollisionHandler, but unlike collision result, overlaps has no result -
/// particles either overlap or not. Note that overlaps are processed before collisions and if two particles
/// do not overlap (\ref overlaps returns false), the check for collisions is skipped.
class IOverlapHandler : public Polymorphic {
public:
    virtual void initialize(Storage& storage) = 0;

    /// \brief Returns true if two particles overlaps
    ///
    /// If so, the overlap is then resolved using \ref handle.
    /// \param i,j Indices of particles in the storage.
    virtual bool overlaps(const Size i, const Size j) const = 0;

    /// \brief Handles the overlap of two particles.
    ///
    /// When called, the particles must actually overlap (\ref overlaps must return true). This is checked by
    /// assert.
    virtual void handle(const Size i, const Size j, FlatSet<Size>& toRemove) = 0;
};

/// \brief Helper function sorting two values
template <typename T>
Tuple<T, T> minMax(const T& t1, const T& t2) {
    if (t1 < t2) {
        return { t1, t2 };
    } else {
        return { t2, t1 };
    }
}

template <typename T>
INLINE T weightedAverage(const T& v1, const Float w1, const T& v2, const Float w2) {
    ASSERT(w1 + w2 > 0._f, w1, w2);
    return (v1 * w1 + v2 * w2) / (w1 + w2);
}

INLINE bool areParticlesBound(const Float m_sum, const Float h_sum, const Vector& dv, const Float limit) {
    const Float vEscSqr = 2._f * Constants::gravity * m_sum / h_sum;
    const Float vRelSqr = getSqrLength(dv);
    return vRelSqr * limit < vEscSqr;
}

// ----------------------------------------------------------------------------------------------------------
// Collision Handlers
// ----------------------------------------------------------------------------------------------------------

/// \brief Helper handler always returning CollisionResult::NONE.
///
/// Cannot be used directly as collision always have to return a valid result (not NONE), but it can be used
/// for testing purposes or in composite handlers (such as \ref FallbackHandler).
class NullCollisionHandler : public ICollisionHandler {
public:
    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual CollisionResult collide(const Size UNUSED(i),
        const Size UNUSED(j),
        FlatSet<Size>& UNUSED(toRemove)) override {
        return CollisionResult::NONE;
    }
};

/// \brief Handler merging particles into a single, larger particles.
///
/// The volume of the merger is the sum of volumes of colliders. Particles are only merged if the relative
/// velocity of collision is lower than the escape velocity and if the angular frequency of the would-be
/// merger is lower than the break-up frequency; if not, CollisionResult::NONE is returned.
class MergingCollisionHandler : public ICollisionHandler {
private:
    ArrayView<Vector> r, v;
    ArrayView<Float> m;
    ArrayView<Vector> L;
    ArrayView<Vector> omega;
    ArrayView<SymmetricTensor> I;
    ArrayView<Tensor> E;

    /// \todo filling factor, collapse of particles in contact must result in sphere of same volume!

    Float bounceLimit;
    Float rotationLimit;

public:
    explicit MergingCollisionHandler(const RunSettings& settings) {
        bounceLimit = settings.get<Float>(RunSettingsId::COLLISION_BOUNCE_MERGE_LIMIT);
        rotationLimit = settings.get<Float>(RunSettingsId::COLLISION_ROTATION_MERGE_LIMIT);
    }

    explicit MergingCollisionHandler(const Float bounceLimit, const Float rotationLimit)
        : bounceLimit(bounceLimit)
        , rotationLimit(rotationLimit) {}

    virtual void initialize(Storage& storage) override {
        ArrayView<Vector> dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        m = storage.getValue<Float>(QuantityId::MASS);
        omega = storage.getValue<Vector>(QuantityId::ANGULAR_FREQUENCY);

        if (storage.has(QuantityId::MOMENT_OF_INERTIA)) {
            I = storage.getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);
            E = storage.getValue<Tensor>(QuantityId::LOCAL_FRAME);
            L = storage.getValue<Vector>(QuantityId::ANGULAR_MOMENTUM);
        }
    }

    virtual CollisionResult collide(const Size i, const Size j, FlatSet<Size>& toRemove) override {
        // set radius of the merger so that the volume is conserved
        const Float h_merger = root<3>(pow<3>(r[i][H]) + pow<3>(r[j][H]));

        // conserve total mass
        const Float m_merger = m[i] + m[j];

        // merge so that the center of mass remains unchanged
        const Vector r_merger = weightedAverage(r[i], m[i], r[j], m[j]);

        // converve momentum
        const Vector v_merger = weightedAverage(v[i], m[i], v[j], m[j]);

        Vector omega_merger;
        SymmetricTensor I_merger;
        Vector L_merger;
        Tensor E_merger;

        // Never modify particle values below UNTIL we know the collision is accepted; save the preliminary
        // values to _merger variables!

        if (I) {
            // So far this is just an experimental branch, not intended to be used for "serious" simulations.
            // This assert is just a check that it does not get enabled by accident.
            ASSERT(Assert::isTest);

            // compute inertia tensors in inertial frame
            const SymmetricTensor I1 = transform(I[i], convert<AffineMatrix>(E[i]));
            const SymmetricTensor I2 = transform(I[j], convert<AffineMatrix>(E[j]));

            // sum up the inertia tensors, but first move them to new origin
            I_merger = Rigid::parallelAxisTheorem(I1, m[i], r_merger - r[i]) +
                       Rigid::parallelAxisTheorem(I2, m[j], r_merger - r[j]);

            // compute the total angular momentum - has to be conserved
            L_merger = m[i] * cross(r[i] - r_merger, v[i] - v_merger) + //
                       m[j] * cross(r[j] - r_merger, v[j] - v_merger) + //
                       L[i] + L[j];
            // L = I*omega  =>  omega = I^-1 * L
            omega_merger = I_merger.inverse() * L_merger;

            // compute the new local frame of the merger and inertia tensor in this frame
            Eigen eigen = eigenDecomposition(I_merger);
            I_merger = SymmetricTensor(eigen.values, Vector(0._f));
            ASSERT(isReal(I_merger));

            E_merger = convert<Tensor>(eigen.vectors);
            ASSERT(isReal(E_merger));

            ASSERT(isReal(L_merger) && isReal(omega_merger), L_merger, omega_merger);
            /// \todo remove, we have unit tests for this
            ASSERT(almostEqual(getSqrLength(E_merger.row(0)), 1._f, 1.e-6_f));
            ASSERT(almostEqual(getSqrLength(E_merger.row(1)), 1._f, 1.e-6_f));
            ASSERT(almostEqual(getSqrLength(E_merger.row(2)), 1._f, 1.e-6_f));

        } else {
            L_merger = m[i] * cross(r[i] - r_merger, v[i] - v_merger) + //
                       m[j] * cross(r[j] - r_merger, v[j] - v_merger) + //
                       Rigid::sphereInertia(m[i], r[i][H]) * omega[i] + //
                       Rigid::sphereInertia(m[j], r[j][H]) * omega[j];
            omega_merger = Rigid::sphereInertia(m_merger, h_merger).inverse() * L_merger;
        }

        if (!this->acceptMerge(i, j, h_merger, omega_merger)) {
            return CollisionResult::NONE;
        }

        // NOW we can save the values

        m[i] = m_merger;
        r[i] = r_merger;
        r[i][H] = h_merger;
        v[i] = v_merger;
        v[i][H] = 0._f;
        omega[i] = omega_merger;

        if (I) {
            I[i] = I_merger;
            L[i] = L_merger;
            E[i] = E_merger;
        }

        ASSERT(isReal(v[i]) && isReal(r[i]));
        toRemove.insert(j);
        return CollisionResult::MERGER;
    }

private:
    /// \brief Checks if the particles should be merged
    ///
    /// We merge particles if their relative velocity is lower than the escape velocity AND if the angular
    /// velocity of the merger is lower than the breakup limit.
    /// \param i, j Particle indices
    /// \param h Radius of the merger
    /// \param omega Angular velocity of the merger
    INLINE bool acceptMerge(const Size i, const Size j, const Float h, const Vector& omega) const {
        if (!areParticlesBound(m[i] + m[j], r[i][H] + r[j][H], v[i] - v[j], bounceLimit)) {
            // moving too fast, reject the merge
            /// \todo shouldn't we check velocities AFTER the bounce?
            return false;
        }
        const Float omegaCritSqr = Constants::gravity * (m[i] + m[j]) / pow<3>(h);
        const Float omegaSqr = getSqrLength(omega);
        if (omegaSqr * rotationLimit > omegaCritSqr) {
            // rotates too fast, reject the merge
            return false;
        }
        return true;
    }
};

/// \brief Handler for bounce on collision.
///
/// No merging takes place. Particles lose fraction of momentum, given by coefficients of restitution.
/// \todo if restitution.t < 1, we should spin up the particles to conserve the angular momentum!!
class ElasticBounceHandler : public ICollisionHandler {
protected:
    ArrayView<Vector> r, v;
    ArrayView<Float> m;

    /// Coefficients of restitution
    struct {
        /// Normal;
        Float n;

        /// Tangential
        Float t;

    } restitution;

public:
    explicit ElasticBounceHandler(const RunSettings& settings) {
        restitution.n = settings.get<Float>(RunSettingsId::COLLISION_RESTITUTION_NORMAL);
        restitution.t = settings.get<Float>(RunSettingsId::COLLISION_RESTITUTION_TANGENT);
    }

    ElasticBounceHandler(const Float n, const Float t) {
        restitution.n = n;
        restitution.t = t;
    }

    virtual void initialize(Storage& storage) override {
        ArrayView<Vector> dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        m = storage.getValue<Float>(QuantityId::MASS);
    }

    virtual CollisionResult collide(const Size i, const Size j, FlatSet<Size>& UNUSED(toRemove)) override {
        const Vector dr = getNormalized(r[i] - r[j]);
        const Vector v_com = weightedAverage(v[i], m[i], v[j], m[j]);
        v[i] = this->reflect(v[i], v_com, -dr);
        v[j] = this->reflect(v[j], v_com, dr);

        // no change of radius
        v[i][H] = 0._f;
        v[j][H] = 0._f;

        ASSERT(isReal(v[i]) && isReal(v[j]));
        return CollisionResult::BOUNCE;
    }

private:
    INLINE Vector reflect(const Vector& v, const Vector& v_com, const Vector& dir) {
        ASSERT(almostEqual(getSqrLength(dir), 1._f), dir);
        const Vector v_rel = v - v_com;
        const Float proj = dot(v_rel, dir);
        const Vector v_t = v_rel - proj * dir;
        const Vector v_n = proj * dir;

        // flip the orientation of normal component (bounce) and apply coefficients of restitution
        return restitution.t * v_t - restitution.n * v_n + v_com;
    }
};

/// \brief Composite handler, choosing another collision handler if the primary handler rejects the
/// collision by returning \ref CollisionResult::NONE.
template <typename TPrimary, typename TFallback>
class FallbackHandler : public ICollisionHandler {
private:
    TPrimary primary;
    TFallback fallback;

public:
    FallbackHandler(const RunSettings& settings)
        : primary(settings)
        , fallback(settings) {}

    virtual void initialize(Storage& storage) override {
        primary.initialize(storage);
        fallback.initialize(storage);
    }

    virtual CollisionResult collide(const Size i, const Size j, FlatSet<Size>& toRemove) override {
        CollisionResult result = primary.collide(i, j, toRemove);
        if (result == CollisionResult::NONE) {
            return fallback.collide(i, j, toRemove);
        } else {
            return result;
        }
    }
};

class FragmentationHandler : public ICollisionHandler {
public:
    // ParametricRelations, directionality of fragments

    virtual CollisionResult collide(const Size i, const Size j, FlatSet<Size>& UNUSED(toRemove)) override {
        (void)i;
        (void)j;
        /// \todo
        NOT_IMPLEMENTED;
        return CollisionResult::FRAGMENTATION;
    }
};


// ----------------------------------------------------------------------------------------------------------
// Overlap Handlers
// ----------------------------------------------------------------------------------------------------------

/// \brief Handler simply ignoring overlaps.
class NullOverlapHandler : public IOverlapHandler {
public:
    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual bool overlaps(const Size UNUSED(i), const Size UNUSED(j)) const override {
        return false;
    }

    virtual void handle(const Size UNUSED(i),
        const Size UNUSED(j),
        FlatSet<Size>& UNUSED(toRemove)) override {}
};

/// \brief Handler unconditionally merging the overlapping particles.
///
/// Behaves similarly to collision handler \ref MergingCollisionHandler, but there is no check for relative
/// velocities of particles nor angular frequency of the merger - particles are always merged.
class MergeOverlapHandler : public IOverlapHandler {
private:
    MergingCollisionHandler handler;

public:
    MergeOverlapHandler()
        : handler(0._f, 0._f) {}

    virtual void initialize(Storage& storage) override {
        handler.initialize(storage);
    }

    virtual bool overlaps(const Size UNUSED(i), const Size UNUSED(j)) const override {
        return true;
    }

    virtual void handle(const Size i, const Size j, FlatSet<Size>& toRemove) override {
        handler.collide(i, j, toRemove);
    }
};

/// \brief Handler displacing the overlapping particles so that they just touch.
///
/// Particles are moved alongside their relative position vector. They must not overlap perfectly, as in such
/// a case the displacement direction is undefined. The handler is templated, allowing to specify a followup
/// handler that is applied after the particles have been moved. This can be another \ref IOverlapHandler, but
/// also a \ref ICollisionHandler, as the two interfaces are similar.
template <typename TFollowupHandler>
class RepelHandler : public IOverlapHandler {
private:
    TFollowupHandler handler;

    ArrayView<Vector> r, v;
    ArrayView<Float> m;

public:
    explicit RepelHandler(const RunSettings& settings)
        : handler(settings) {}

    virtual void initialize(Storage& storage) override {
        ArrayView<Vector> dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        m = storage.getValue<Float>(QuantityId::MASS);

        handler.initialize(storage);
    }

    virtual bool overlaps(const Size UNUSED(i), const Size UNUSED(j)) const override {
        // this function is called only if the spheres intersect, which is the only condition for this handler
        return true;
    }

    virtual void handle(const Size i, const Size j, FlatSet<Size>& toRemove) override {
        Vector dir;
        Float dist;
        tieToTuple(dir, dist) = getNormalizedWithLength(r[i] - r[j]);
        dir[H] = 0._f; // don't mess up radii
        // can be only used for overlapping particles
        ASSERT(dist < r[i][H] + r[j][H], dist, r[i][H] + r[i][H]);
        const Float x1 = (r[i][H] + r[j][H] - dist) / (1._f + m[i] / m[j]);
        const Float x2 = m[i] / m[j] * x1;
        r[i] += dir * x1;
        r[j] -= dir * x2;
        ASSERT(almostEqual(getSqrLength(r[i] - r[j]), sqr(r[i][H] + r[j][H])),
            getSqrLength(r[i] - r[j]),
            sqr(r[i][H] + r[j][H]));

        ASSERT(isReal(v[i]) && isReal(v[j]));
        ASSERT(isReal(r[i]) && isReal(r[j]));

        // Now when the two particles are touching, handle the collision using the followup handler.
        handler.collide(i, j, toRemove);
    }
};

/// \brief Overlap handler performing a bounce of particles.
///
/// Particles only bounce if their centers are moving towards each other. This way we prevent particles
/// bouncing infinitely - after the first bounce, the particle move away from each other and they are not
/// classified as overlaps by the handler.
class InternalBounceHandler : public IOverlapHandler {
private:
    ElasticBounceHandler handler;

    ArrayView<Vector> r, v;

public:
    explicit InternalBounceHandler(const RunSettings& settings)
        : handler(settings) {
        // this handler allows overlaps of particles, so it should never be used with point particles, as we
        // could potentially get infinite accelerations
        ASSERT(settings.get<GravityKernelEnum>(RunSettingsId::GRAVITY_KERNEL) !=
               GravityKernelEnum::POINT_PARTICLES);
    }

    virtual void initialize(Storage& storage) override {
        ArrayView<Vector> dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

        handler.initialize(storage);
    }

    virtual bool overlaps(const Size i, const Size j) const override {
        // overlap needs to be handled only if the particles are moving towards each other
        const Vector dr = r[i] - r[j];
        const Vector dv = v[i] - v[j];
        return dot(dr, dv) < 0._f;
    }

    virtual void handle(const Size i, const Size j, FlatSet<Size>& toRemove) override {
        handler.collide(i, j, toRemove);
    }
};

/// \brief Handler merging overlapping particles if their relative velocity is lower than the escape velocity.
///
/// If the relative veloity is too high, the particles are allowed to simply pass each other - this situation
/// is not classified as overlap.
class MergeBoundHandler : public IOverlapHandler {
private:
    MergingCollisionHandler handler;

public:
    ArrayView<Vector> r, v, omega;
    ArrayView<Float> m;

    Float bounceLimit;
    Float rotationLimit;

public:
    explicit MergeBoundHandler(const RunSettings& settings)
        : handler(settings) {
        bounceLimit = settings.get<Float>(RunSettingsId::COLLISION_BOUNCE_MERGE_LIMIT);
        rotationLimit = settings.get<Float>(RunSettingsId::COLLISION_ROTATION_MERGE_LIMIT);
    }

    virtual void initialize(Storage& storage) override {
        ArrayView<Vector> dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        omega = storage.getValue<Vector>(QuantityId::ANGULAR_FREQUENCY);
        m = storage.getValue<Float>(QuantityId::MASS);

        handler.initialize(storage);
    }

    virtual bool overlaps(const Size i, const Size j) const override {
        const Float m_merger = m[i] + m[j];
        if (!areParticlesBound(m_merger, r[i][H] + r[j][H], v[i] - v[j], bounceLimit)) {
            // moving too fast, reject the merge
            /// \todo shouldn't we check velocities AFTER the bounce?
            return false;
        }

        const Float h_merger = root<3>(pow<3>(r[i][H]) + pow<3>(r[j][H]));
        const Float omegaCritSqr = Constants::gravity * (m[i] + m[j]) / pow<3>(h_merger);

        const Vector r_merger = weightedAverage(r[i], m[i], r[j], m[j]);
        const Vector v_merger = weightedAverage(v[i], m[i], v[j], m[j]);
        const Vector L_merger = m[i] * cross(r[i] - r_merger, v[i] - v_merger) + //
                                m[j] * cross(r[j] - r_merger, v[j] - v_merger) + //
                                Rigid::sphereInertia(m[i], r[i][H]) * omega[i] + //
                                Rigid::sphereInertia(m[j], r[j][H]) * omega[j];
        const Vector omega_merger = Rigid::sphereInertia(m_merger, h_merger).inverse() * L_merger;
        const Float omegaSqr = getSqrLength(omega_merger);
        if (omegaSqr * rotationLimit > omegaCritSqr) {
            // rotates too fast, reject the merge
            return false;
        }

        return true;
    }

    virtual void handle(const Size i, const Size j, FlatSet<Size>& toRemove) override {
        handler.collide(i, j, toRemove);
    }
};

NAMESPACE_SPH_END
