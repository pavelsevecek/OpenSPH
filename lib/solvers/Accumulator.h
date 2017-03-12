#pragma once

#include "objects/containers/Array.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN


/// Simple wrapper over array to simplify accumulating values for each interacting pair of particles.
/// This is useful for quantities where the value is determined by direct summation over neighbouring
/// particles rather than by solving evolutionary equation.

template <typename TAccumulate>
class Accumulator : public Noncopyable {
private:
    using Value = typename TAccumulate::Type;

    Array<Value> values;
    TAccumulate functor;
    Optional<QuantityIds> key = NOTHING;

public:
    Accumulator() = default;

    Accumulator(const QuantityIds key)
        : key(key) {}

    Accumulator(Accumulator&& other)
        : values(std::move(other.values))
        , functor(std::move(other.functor)) {}

    void update(Storage& storage) {
        values.resize(storage.getParticleCnt());
        values.fill(Value(0._f));
        functor.template update(storage);
    }

    /// Accumulate quantity for a pair of particles. This function should only be called once for each pair of
    /// particles; for particles i and j, functor returns Tuple<TValue, TValue> accumulating quantity
    /// increment of i-th and j-th particle.
    INLINE void accumulate(const Size i, const Size j, const Vector& grad) {
        Value v1, v2;
        tieToTuple(v1, v2) = functor(i, j, grad);
        values[i] += v1;
        values[j] += v2;
    }

    void integrate(Storage& storage) {
        if (key && storage.has<Value, OrderEnum::ZERO>(key.get())) {
            Array<Value>& quantity = storage.getValue<Value>(key.get());
            quantity.swap(values);
        }
    }

    INLINE operator Array<Value>&() {
        return values;
    }

    INLINE operator const Array<Value>&() const {
        return values;
    }

    INLINE Value& operator[](const int idx) {
        return values[idx];
    }
};

/// Base class for all object owning accumulators. Template parameter pack shall contain types of all
/// accumulators owned by the object.
template <typename... TAccumulate>
class AccumulatorOwner : public Noncopyable {
private:
    Tuple<Accumulator<TAccumulate>&...> accumulators;

public:
    using Accumulating = Tuple<TAccumulate...>;

    /// Constructor taking references to arrays there the accumulated values are stored
    AccumulatorOwner(Accumulator<TAccumulate>&... accumulators)
        : accumulators(accumulators...) {}

    /// Return a reference to array given by accumulator type.
    template <typename TFunctor>
    Array<typename TFunctor::Type>& get() {
        return accumulators.template get<Accumulator<TFunctor>&>();
    }

    INLINE void accumulate(const Size i, const Size j, const Vector& grad) {
        forEach(accumulators, [i, j, &grad](auto& ac) { ac.accumulate(i, j, grad); });
    }
};

/// \todo better names for accumulator, functors (inside accumulators) and owners
/// Velocity divergence
class DivvImpl {
private:
    ArrayView<const Vector> v;
    ArrayView<const Float> m;
    ArrayView<const Float> rho;

public:
    using Type = Float;

    void update(Storage& storage) {
        tie(m, rho) = storage.getValues<Float>(QuantityIds::MASSES, QuantityIds::DENSITY);
        v = storage.getDt<Vector>(QuantityIds::POSITIONS);
    }

    INLINE Tuple<Float, Float> operator()(const int i, const int j, const Vector& grad) const {
        const Float delta = dot(v[j] - v[i], grad);
        ASSERT(isReal(delta));
        return { m[j] / rho[j] * delta, m[i] / rho[i] * delta };
    }
};
using Divv = Accumulator<DivvImpl>;

/// Velocity rotation
class RotvImpl {
private:
    ArrayView<const Vector> v;
    ArrayView<const Float> m;
    ArrayView<const Float> rho;

public:
    using Type = Vector;

    void update(Storage& storage) {
        tie(m, rho) = storage.getValues<Float>(QuantityIds::MASSES, QuantityIds::DENSITY);
        v = storage.getDt<Vector>(QuantityIds::POSITIONS);
    }

    INLINE Tuple<Vector, Vector> operator()(const int i, const int j, const Vector& grad) const {
        const Vector rot = cross(v[j] - v[i], grad);
        ASSERT(isReal(rot));
        return { m[j] / rho[j] * rot, m[i] / rho[i] * rot };
    }
};
using Rotv = Accumulator<RotvImpl>;

/// Velocity divergence multiplied by density (right-hand side of continuity equation)
class RhoDivvImpl {
private:
    ArrayView<const Float> m;
    ArrayView<const Vector> v;
    ArrayView<const Float> rho;

public:
    using Type = Float;

    void update(Storage& storage) {
        m = storage.getValue<Float>(QuantityIds::MASSES);
        v = storage.getDt<Vector>(QuantityIds::POSITIONS);
        rho = storage.getValue<Float>(QuantityIds::DENSITY);
    }

    INLINE Tuple<Float, Float> operator()(const int i, const int j, const Vector& grad) const {
        const Float delta = dot(v[j] - v[i], grad);
        ASSERT(isReal(delta));
        return { m[j] / rho[j] * delta, m[i] / rho[i] * delta };
    }
};
using RhoDivv = Accumulator<RhoDivvImpl>;

/// Strain rate tensor (symmetrized velocity gradient) multiplied by density
/// \todo rename to describe we only accumulate particles belonging to the same body
class RhoGradvImpl {
private:
    ArrayView<const Float> m;
    ArrayView<const Vector> v;
    ArrayView<const Float> rho;
    ArrayView<const Float> dmg, reducing;
    ArrayView<const Size> idxs;

public:
    using Type = Tensor;

    void update(Storage& storage) {
        m = storage.getValue<Float>(QuantityIds::MASSES);
        v = storage.getAll<Vector>(QuantityIds::POSITIONS)[1];
        idxs = storage.getValue<Size>(QuantityIds::FLAG);
        rho = storage.getValue<Float>(QuantityIds::DENSITY);
        if (storage.has(QuantityIds::DAMAGE)) {
            dmg = storage.getValue<Float>(QuantityIds::DAMAGE);
        }
        if (storage.has(QuantityIds::YIELDING_REDUCE)) {
            reducing = storage.getValue<Float>(QuantityIds::YIELDING_REDUCE);
        }
    }

    INLINE Tuple<Tensor, Tensor> operator()(const int i, const int j, const Vector& grad) const {
        Float redi = reducing[i], redj = reducing[j];
        if (dmg) {
            redi *= (1._f - pow<3>(dmg[i]));
            redj *= (1._f - pow<3>(dmg[j]));
        }
        if (idxs[i] != idxs[j] || redi == 0._f || redj == 0._f) {
            /// \todo optimize, instead of returning zero, solve this directly on accumulators/modules
            /// (somehow)
            return { Tensor::null(), Tensor::null() };
        }
        const Tensor gradv = outer(v[j] - v[i], grad);
        ASSERT(isReal(gradv));
        return { m[j] / rho[j] * gradv, m[i] / rho[i] * gradv };
    }
};
using RhoGradv = Accumulator<RhoGradvImpl>;


/// Average direction of neighbouring particles. This results to zero vector to order O(h^2) for particles
/// inside a body and to nonzero vector to boundary particles. Direction of the vector is an approximation of
/// surface (inward) normal.
class SurfaceNormalImpl {
private:
    ArrayView<const Vector> r;
    ArrayView<const Size> flag;

public:
    using Type = Vector;

    void update(Storage& storage) {
        r = storage.getValue<Vector>(QuantityIds::POSITIONS);
        flag = storage.getValue<Size>(QuantityIds::FLAG);
    }

    INLINE Tuple<Vector, Vector> operator()(const int i, const int j, const Vector& UNUSED(grad)) const {
        const Vector dr = r[j] - r[i];
        const Float length = getLength(dr);
        if (flag[i] != flag[j] || length == 0._f) {
            return { Vector(0._f), Vector(0._f) };
        }
        const Vector normalized = dr / length;
        return { normalized, -normalized };
    }
};
using SurfaceNormal = Accumulator<SurfaceNormalImpl>;

/// Correction tensor ensuring conversion of total angular momentum to the first order.
/// The accumulated value is an inversion of the correction C_ij, applied as multiplicative factor on velocity
/// gradient.
/// \todo They use slightly different SPH equations, check that it's ok
/// \todo It is really symmetric tensor?
class SchaferEtAlCorrectionImpl {
private:
    ArrayView<const Float> m;
    ArrayView<const Float> rho;
    ArrayView<const Vector> r;

public:
    using Type = Tensor;

    void update(Storage& storage) {
        m = storage.getValue<Float>(QuantityIds::MASSES);
        r = storage.getValue<Vector>(QuantityIds::POSITIONS);
        rho = storage.getValue<Float>(QuantityIds::DENSITY);
    }

    INLINE Tuple<Tensor, Tensor> operator()(const int i, const int j, const Vector& grad) const {
        Tensor t = outer(r[j] - r[i], grad); // symmetric in i,j ?
        return { m[j] / rho[j] * t, m[i] / rho[i] * t };
    }
};
using SchaferEtAlCorrection = Accumulator<SchaferEtAlCorrectionImpl>;

NAMESPACE_SPH_END
