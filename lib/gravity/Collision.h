#pragma once

/// \file Collision.h
/// \brief Collision handling
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "physics/Functions.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

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
    /// \param t Remaining time of the step (after the collision)
    /// \return True if the collision took place, false to reject the collision.
    virtual bool collide(const Size i, const Size j, const Float t) = 0;
};

/// \brief Helper class, similar to ArrayView, but actually holding a reference to the array itself.
///
/// The advantage over ArrayView is that it doesn't get invalidated when the array is resized. Works only with
/// Array, no other container.
template <typename T>
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
};

class PerfectMergingHandler : public ICollisionHandler {
private:
    RawPtr<Storage> storage;

    ArrayRef<Vector> r, v;
    ArrayRef<Float> m;

public:
    virtual void initialize(Storage& storage) override {
        this->storage = std::addressof(storage);
        ArrayRef<Vector> dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

        m = storage.getValue<Float>(QuantityId::MASS);
    }

    virtual bool collide(const Size i, const Size j, const Float t_rest) override {
        // set radius of the merger to conserve volume
        const Float h_merger = root<3>(pow<3>(r[i][H]) + pow<3>(r[j][H]));
        const Float m_merger = m[i] + m[j];

        const Vector r_merger = weightedAverage(r[i], m[i], r[j], m[j]);
        const Vector v_merger = weightedAverage(v[i], m[i], v[j], m[j]);

        v[i] = v_merger;
        r[i] = r_merger + v[i] * t_rest;
        r[i][H] = h_merger;
        m[i] = m_merger;

        // remove the j-th particle
        storage->remove(Array<Size>{ j }, true);
        return true;
    }

private:
    template <typename T>
    INLINE static T weightedAverage(const T& v1, const Float w1, const T& v2, const Float w2) {
        ASSERT(w1 + w2 > 0._f, w1, w2);
        return (v1 * w1 + v2 * w2) / (w1 + w2);
    }
};

class ElasticBounceHandler : public ICollisionHandler {
private:
    ArrayRef<Vector> r, v;
    ArrayRef<Float> m;

    /// Coefficients of restitution
    struct {
        /// Normal;
        Float n;

        /// Tangential
        Float t;
    } restitution;

public:
    ElasticBounceHandler(const Float n, const Float t)
        : restitution{ n, t } {}

    virtual void initialize(Storage& storage) override {
        ArrayRef<Vector> dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

        m = storage.getValue<Float>(QuantityId::MASS);
    }

    virtual bool collide(const Size i, const Size j, const Float t_rest) override {
        const Vector dr = getNormalized(r[i] - r[j]);
        v[i] = this->reflect(v[i], -dr);
        v[j] = this->reflect(v[j], dr);

        r[i] += v[i] * t_rest;
        r[j] += v[j] * t_rest;
        return true;
    }

private:
    INLINE Vector reflect(const Vector& v, const Vector& dir) {
        ASSERT(almostEqual(getSqrLength(dir), 1._f), dir);
        const Float proj = dot(v, dir);
        const Vector v_t = v - proj * dir;
        const Vector v_n = proj * dir;

        // flip the orientation of normal component (bounce) and apply coefficients of restitution
        return restitution.t * v_t - restitution.n * v_n;
    }
};


class FragmentationHandler : public ICollisionHandler {
public:
    // ParametricRelations, directionality of fragments

    virtual bool collide(const Size i, const Size j, const Float t) override {
        (void)i;
        (void)j;
        (void)t;
        /// \todo
        return true;
    }
};

/// \brief Auxiliary collision handler, distinguising the collision behavior based on impact energy
///
/// \todo generalize scaling law?
class ThresholdHandler : public ICollisionHandler {
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

    virtual bool collide(const Size i, const Size j, const Float t_rest) override {
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
            return fast->collide(i, j, t_rest);
        } else {
            return slow->collide(i, j, t_rest);
        }
    }
};

NAMESPACE_SPH_END
