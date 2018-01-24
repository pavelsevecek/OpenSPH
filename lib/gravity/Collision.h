#pragma once

/// \file Collision.h
/// \brief Collision handling
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "physics/Functions.h"
#include "quantities/Storage.h"
#include "system/Settings.h"
#include <set>

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
    virtual CollisionResult collide(const Size i, const Size j, std::set<Size>& toRemove) = 0;
};

/// \brief Helper class, similar to ArrayView, but actually holding a reference to the array itself.
///
/// The advantage over ArrayView is that it doesn't get invalidated when the array is resized. Works only with
/// Array, no other container.
/*template <typename T>
class ArrayRef {
private:
    RawPtr<Array<T>> ptr;

public:
    ArrayRef() = default;

    ArrayRef(Array<T>& array)
        : ptr(std::addressof(array)) {}

    ArrayRef& operator=(Array<T>& array) {
        ptr = std::addressof(array);
        return *this;
    }

    INLINE T& operator[](const Size index) {
        return (*ptr)[index];
    }

    INLINE const T& operator[](const Size index) const {
        return (*ptr)[index];
    }

    INLINE Size size() const {
        return ptr->size();
    }

    operator Array<T>&() {
        return *ptr;
    }

    operator const Array<T>&() const {
        return *ptr;
    }
};*/

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

class NullHandler : public ICollisionHandler {
public:
    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual CollisionResult collide(const Size UNUSED(i),
        const Size UNUSED(j),
        std::set<Size>& UNUSED(toRemove)) override {
        return CollisionResult::NONE;
    }
};

class PerfectMergingHandler : public ICollisionHandler {
private:
    ArrayView<Vector> r, v;
    ArrayView<Vector> omega;
    ArrayView<Float> m;
    ArrayView<SymmetricTensor> I;

    Float mergingLimit;

public:
    explicit PerfectMergingHandler(const RunSettings& settings) {
        mergingLimit = settings.get<Float>(RunSettingsId::COLLISION_MERGING_LIMIT);
    }

    explicit PerfectMergingHandler(const Float mergingLimit)
        : mergingLimit(mergingLimit) {}

    virtual void initialize(Storage& storage) override {
        ArrayView<Vector> dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        omega = storage.getValue<Vector>(QuantityId::ANGULAR_VELOCITY);

        m = storage.getValue<Float>(QuantityId::MASS);
        I = storage.getValue<SymmetricTensor>(QuantityId::MOMENT_OF_INERTIA);
    }

    virtual CollisionResult collide(const Size i, const Size j, std::set<Size>& toRemove) override {
        // set radius of the merger so that the volume is conserved
        const Float h_merger = r[i][H]; // root<3>(pow<3>(r[i][H]) + pow<3>(r[j][H]));

        // conserve total mass
        const Float m_merger = m[i] + m[j];

        // merge so that the center of mass remains unchanged
        const Vector r_merger = r[i]; // weightedAverage(r[i], m[i], r[j], m[j]);

        // converve momentum
        const Vector v_merger = weightedAverage(v[i], m[i], v[j], m[j]);

        // sum up the inertia tensors, but first move them to new origin
        const SymmetricTensor I_merger = Rigid::parallelAxisTheorem(I[i], m[i], r_merger - r[i]) +
                                         Rigid::parallelAxisTheorem(I[j], m[j], r_merger - r[j]);

        // conserve angular momentum
        const Vector L_merger = m[i] * cross(r[i] - r_merger, v[i] - v_merger) + //
                                m[j] * cross(r[j] - r_merger, v[j] - v_merger) + //
                                I[i] * omega[i] + I[j] * omega[j];
        // L = I*omega  =>  omega = I^-1 * L
        const Vector omega_merger = I_merger.inverse() * L_merger;

        if (!this->acceptMerge(i, j, h_merger, omega_merger)) {
            return CollisionResult::NONE;
        }

        /// \todo keep track of density as well?

        m[i] = m_merger;
        r[i] = r_merger;
        r[i][H] = h_merger;
        v[i] = v_merger;
        v[i][H] = 0._f;
        I[i] = I_merger;
        omega[i] = omega_merger;

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
        const Float vEscSqr = 2._f * Constants::gravity * (m[i] + m[j]) / (r[i][H] + r[j][H]);
        const Float vRelSqr = getSqrLength(v[i] - v[j]);
        if (vRelSqr * mergingLimit > vEscSqr) {
            // moving too fast, reject the merge
            /// \todo shouldn't we check velocities AFTER the bounce?
            return false;
        }
        const Float omegaCritSqr = Constants::gravity * (m[i] + m[j]) / pow<3>(h);
        const Float omegaSqr = getSqrLength(omega);
        if (omegaSqr * mergingLimit > omegaCritSqr) {
            // rotates too fast, reject the merge
            return false;
        }
        return true;
    }
};

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

    virtual CollisionResult collide(const Size i, const Size j, std::set<Size>& UNUSED(toRemove)) override {
        const Vector dr = getNormalized(r[i] - r[j]);
        const Vector v_com = (m[i] * v[i] + m[j] * v[j]) / (m[i] + m[j]);
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

class RepelHandler : public ElasticBounceHandler {
public:
    RepelHandler()
        : ElasticBounceHandler(0.1_f, 1._f) {}

    virtual CollisionResult collide(const Size i, const Size j, std::set<Size>& toRemove) override {
        Vector dir;
        Float dist;
        tieToTuple(dir, dist) = getNormalizedWithLength(r[i] - r[j]);
        dir[H] = 0._f;                    // don't mess up radii
        ASSERT(dist < r[i][H] + r[j][H]); // can be only used for overlapping particles
        const Float x1 = (r[i][H] + r[j][H] - dist) / (1._f + m[i] / m[j]);
        const Float x2 = m[i] / m[j] * x1;
        r[i] += dir * x1;
        r[j] -= dir * x2;
        ASSERT(almostEqual(getSqrLength(r[i] - r[j]), sqr(r[i][H] + r[j][H])),
            getSqrLength(r[i] - r[j]),
            sqr(r[i][H] + r[j][H]));

        // set normal velocities to zero to avoid 'accumulating' huge velocities during overlap
        ElasticBounceHandler::collide(i, j, toRemove);
        return CollisionResult::BOUNCE;
    }
};

/// \brief Helper handler, choosing another collision handler if the primary handler rejects the collision.
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

    virtual CollisionResult collide(const Size i, const Size j, std::set<Size>& toRemove) override {
        CollisionResult result = primary.collide(i, j, toRemove);
        if (result == CollisionResult::NONE) {
            ASSERT(toRemove.empty());
            return fallback.collide(i, j, toRemove);
        } else {
            return result;
        }
    }
};

class FragmentationHandler : public ICollisionHandler {
public:
    // ParametricRelations, directionality of fragments

    virtual CollisionResult collide(const Size i, const Size j, std::set<Size>& UNUSED(toRemove)) override {
        (void)i;
        (void)j;
        /// \todo
        NOT_IMPLEMENTED;
        return CollisionResult::FRAGMENTATION;
    }
};

/// \brief Auxiliary collision handler, distinguising the collision behavior based on impact energy
///
/// \todo generalize scaling law?
/*class ThresholdHandler : public ICollisionHandler {
private:
    AutoPtr<ICollisionHandler> slow;
    AutoPtr<ICollisionHandler> fast;
    Float thresholdSqr;

    ArrayRef<Float> m;
    ArrayRef<Vector> r, v;

public:
    ThresholdHandler(const Float threshold,
        AutoPtr<ICollisionHandler>&& slow,
        AutoPtr<ICollisionHandler>&& fast)
        : slow(std::move(slow))
        , fast(std::move(fast))
        , thresholdSqr(sqr(threshold)) {}

    virtual void initialize(Storage& storage) override {
        slow->initialize(storage);
        fast->initialize(storage);

        ArrayRef<Vector> dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        m = storage.getValue<Float>(QuantityId::MASS);
    }

    virtual CollisionResult collide(const Size i, const Size j, Array<Size>& toRemove) override {
        const Float dv2 = getSqrLength(v[i] - v[j]);
        // determine the target and the impactor
        Float M_targ, m_imp, D_targ;
        if (m[i] >= m[j]) {
            M_targ = m[i];
            D_targ = 2._f * r[i][H];
            m_imp = m[j];
        } else {
            M_targ = m[j];
            D_targ = 2._f * r[j][H];
            m_imp = m[i];
        }
        const Float Q = 0.5_f * m_imp * dv2 / M_targ;
        /// \todo generalize energy
        const Float Q_D = evalBenzAsphaugScalingLaw(D_targ, 2700._f);
        if (Q / Q_D > thresholdSqr) {
            return fast->collide(i, j, toRemove);
        } else {
            return slow->collide(i, j, toRemove);
        }
    }
};*/

NAMESPACE_SPH_END
