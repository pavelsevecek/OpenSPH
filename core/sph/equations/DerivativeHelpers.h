#pragma once

#include "quantities/Quantity.h"
#include "sph/equations/Derivative.h"
#include "objects/containers/Tuple.h"

#ifdef SPH_WIN
#if defined(IGNORE)
#undef IGNORE
#endif
#endif

NAMESPACE_SPH_BEGIN

enum class DerivativeFlag {
    /// \brief Use correction tensor on kernel gradient when evaluating the derivative.
    ///
    /// Can be currently only used for asymmetric derivatives. Implies \ref SUM_ONLY_UNDAMAGED - the
    /// correction tensor is only computed for undamaged particles.
    CORRECTED = 1 << 0,

    /// \brief Only undamaged particles (particles with damage > 0) from the same body (body with the same
    /// flag) will contribute to the sum.
    SUM_ONLY_UNDAMAGED = 1 << 1,
};

/// \brief Helper template for derivatives that define both the symmetrized and asymmetric variant.
///
/// This class template is mainly used to reduce the boilerplate code. It allows to easily implement the \ref
/// ISymmetricDerivative interface by defining a single function; the derived class must implement \a eval
/// templated member function, taking the indices of the particles and the gradient as arguments. The template
/// parameter is a bool, where false means asymmetric evaluation (only the first particle should be modified),
/// and true means symmetric evaluation. The loop over the real neighbors (whether all neighbors or just
/// undamaged particles is specified by the flags passed in the constructor) is automatically performed by
/// \ref DerivativeTemplate and does not have to be re-implemented by the derived classes.
///
/// Derived class must also implement functions \a additionalCreate and \a additionalInitialize. These
/// functions have the same signatures are their virtual counterparts in \ref IDerivative, they are used to
/// initialize additional ArrayViews and other parameters specific for the derived class, without the need to
/// override the virtual function and manually call the function of a parent (which is why the functions are
/// marked as final).
///
/// \note It is mandatory to implement the additional* function, even if the implementation is empty, so that
/// it is ensured the correct function is called and avoid subtle bugs if the "overriding" function is
/// misspelled.
template <typename TDerived>
class DerivativeTemplate : public ISymmetricDerivative {
private:
    ArrayView<const Size> idxs;
    ArrayView<const Float> reduce;
    ArrayView<const SymmetricTensor> C;

    Flags<DerivativeFlag> flags;

public:
    explicit DerivativeTemplate(const RunSettings& settings, const Flags<DerivativeFlag> flags = EMPTY_FLAGS)
        : flags(flags) {
        const bool useCorrectionTensor = settings.get<bool>(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR);
        if (!useCorrectionTensor) {
            // 'global' override for correction tensor
            this->flags.unset(DerivativeFlag::CORRECTED);
        }
        const bool sumOnlyUndamaged = settings.get<bool>(RunSettingsId::SPH_SUM_ONLY_UNDAMAGED);
        if (!sumOnlyUndamaged) {
            // 'global' override - always sum all particles
            this->flags.unset(DerivativeFlag::SUM_ONLY_UNDAMAGED);
        }
    }

    virtual void create(Accumulated& results) override final {
        derived()->additionalCreate(results);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override final {
        if (flags.has(DerivativeFlag::CORRECTED)) {
            C = results.getBuffer<SymmetricTensor>(
                QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO);
        } else {
            C = nullptr;
        }
        if (flags.has(DerivativeFlag::SUM_ONLY_UNDAMAGED) && input.has(QuantityId::STRESS_REDUCING)) {
            idxs = input.getValue<Size>(QuantityId::FLAG);
            reduce = input.getValue<Float>(QuantityId::STRESS_REDUCING);
        } else {
            idxs = nullptr;
            reduce = nullptr;
        }

        derived()->additionalInitialize(input, results);
    }

    virtual bool equals(const IDerivative& other) const override final {
        if (typeid(*this) != typeid(other)) {
            return false;
        }
        const TDerived* actOther = assertCast<const TDerived>(&other);
        return (flags == actOther->flags) && derived()->additionalEquals(*actOther);
    }

    virtual void evalNeighs(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) override {
        SPH_ASSERT(neighs.size() == grads.size());
        if (C) {
            sum(idx, neighs, grads, [this](Size i, Size j, const Vector& grad) INL {
                SPH_ASSERT(C[i] != SymmetricTensor::null());
                derived()->template eval<false>(i, j, C[i] * grad);
            });
        } else {
            sum(idx, neighs, grads, [this](Size i, Size j, const Vector& grad) INL {
                derived()->template eval<false>(i, j, grad);
            });
        }
    }

    virtual void evalSymmetric(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) override {
        SPH_ASSERT(neighs.size() == grads.size());
        SPH_ASSERT(!flags.has(DerivativeFlag::CORRECTED));
        sum(idx, neighs, grads, [this](Size i, Size j, const Vector& grad) INL {
            derived()->template eval<true>(i, j, grad);
        });
    }

private:
    template <typename TFunctor>
    INLINE void sum(const Size i,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads,
        const TFunctor& functor) {
        if (reduce) {
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                if (idxs[i] != idxs[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
                    continue;
                }
                functor(i, j, grads[k]);
            }
        } else {
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                functor(i, j, grads[k]);
            }
        }
    }

    TDerived* derived() {
        return static_cast<TDerived*>(this);
    }

    const TDerived* derived() const {
        return static_cast<const TDerived*>(this);
    }
};


/// \brief Helper template specifically used to implement forces.
///
/// Similarly to \ref DerivativeTemplate, this is mainly used to reduce the boilerplate code and avoid error
/// by implementing the member functions of \ref IAcceleration inconsistently. Derived must implement
/// Tuple<Vector, Float> eval(), returning force and heating.
///
/// Acceleration is never corrected! That would break the conservation of momentum.
template <typename TDerived>
class AccelerationTemplate : public IAcceleration {
private:
    ArrayView<Vector> dv;
    ArrayView<Float> du;
    ArrayView<const Float> m;
    ArrayView<const Size> idxs;
    ArrayView<const Float> reduce;
    bool sumOnlyUndamaged;

public:
    explicit AccelerationTemplate(const RunSettings& settings,
        const Flags<DerivativeFlag> flags = EMPTY_FLAGS) {
        SPH_ASSERT(!flags.has(DerivativeFlag::CORRECTED), "Forces should be never corrected");

        // sum only undamaged if specified by the flag and it is specified by the 'global' override
        sumOnlyUndamaged = flags.has(DerivativeFlag::SUM_ONLY_UNDAMAGED) &&
                           settings.get<bool>(RunSettingsId::SPH_SUM_ONLY_UNDAMAGED);
    }

    virtual void create(Accumulated& results) override final {
        results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, BufferSource::SHARED);
        results.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, BufferSource::SHARED);

        derived()->additionalCreate(results);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override final {
        dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
        du = results.getBuffer<Float>(QuantityId::ENERGY, OrderEnum::FIRST);

        m = input.getValue<Float>(QuantityId::MASS);
        if (sumOnlyUndamaged && input.has(QuantityId::STRESS_REDUCING)) {
            idxs = input.getValue<Size>(QuantityId::FLAG);
            reduce = input.getValue<Float>(QuantityId::STRESS_REDUCING);
        } else {
            idxs = nullptr;
            reduce = nullptr;
        }

        derived()->additionalInitialize(input, results);
    }

    virtual bool equals(const IDerivative& other) const override final {
        if (typeid(*this) != typeid(other)) {
            return false;
        }
        const TDerived* actOther = assertCast<const TDerived>(&other);
        return (sumOnlyUndamaged == actOther->sumOnlyUndamaged) && derived()->additionalEquals(*actOther);
    }

    virtual void evalNeighs(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) override {
        SPH_ASSERT(neighs.size() == grads.size());
        sum(idx, neighs, grads, [this](Size UNUSED(k), Size i, Size j, const Vector& grad) INL {
            Vector f;
            Float de;
            tieToTuple(f, de) = derived()->template eval<false>(i, j, grad);
            dv[i] += m[j] * f;
            du[i] += m[j] * de;
        });
    }

    virtual void evalSymmetric(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) override {
        SPH_ASSERT(neighs.size() == grads.size());
        sum(idx, neighs, grads, [this](Size UNUSED(k), Size i, Size j, const Vector& grad) INL {
            Vector f;
            Float de;
            tieToTuple(f, de) = derived()->template eval<true>(i, j, grad);
            dv[i] += m[j] * f;
            dv[j] -= m[i] * f;
            du[i] += m[j] * de;
            du[j] += m[i] * de;
        });
    }

    virtual void evalAcceleration(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads,
        ArrayView<Vector> dv) override {
        SPH_ASSERT(neighs.size() == grads.size() && neighs.size() == dv.size());
        sum(idx, neighs, grads, [this, &dv](Size k, Size i, Size j, const Vector& grad) INL {
            Vector f;
            tieToTuple(f, IGNORE) = derived()->template eval<false>(i, j, grad);
            dv[k] += m[j] * f;
        });
    }

private:
    template <typename TFunctor>
    INLINE void sum(const Size i,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads,
        const TFunctor& functor) {
        if (reduce) {
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                if (idxs[i] != idxs[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
                    continue;
                }
                functor(k, i, j, grads[k]);
            }
        } else {
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                functor(k, i, j, grads[k]);
            }
        }
    }

    INLINE TDerived* derived() {
        return static_cast<TDerived*>(this);
    }

    INLINE const TDerived* derived() const {
        return static_cast<const TDerived*>(this);
    }
};

/// \brief Discretization using the density of the center particle.
///
/// Represents:
/// 1/rho[i] sum_j m[j]*(v[j]-v[i]) * grad_ji
/// This is the discretization of velocity divergence (and other gradients) in standard SPH formulation.
class CenterDensityDiscr {
    ArrayView<const Float> rho, m;

public:
    void initialize(const Storage& input) {
        tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS);
    }

    template <typename T>
    INLINE T eval(const Size i, const Size j, const T& value) {
        return m[j] / rho[i] * value;
    }
};

/// \brief Discretization using the densities of summed particles.
///
/// Represents:
/// sum_j m[j]/rho[j]*(v[j]-v[i]) * grad_ji
/// This is the discretization used in SPH5 code/
class NeighborDensityDiscr {
    ArrayView<const Float> rho, m;

public:
    void initialize(const Storage& input) {
        tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS);
    }

    template <typename T>
    INLINE T eval(const Size UNUSED(i), const Size j, const T& value) {
        return m[j] / rho[j] * value;
    }
};


template <QuantityId Id, typename Discr, typename Traits>
class VelocityTemplate : public DerivativeTemplate<VelocityTemplate<Id, Discr, Traits>> {
private:
    /// Velocity buffer
    ArrayView<const Vector> v;

    /// Discretization type
    Discr discr;

    /// Target buffer where derivatives are written
    ArrayView<typename Traits::Type> deriv;

public:
    using DerivativeTemplate<VelocityTemplate<Id, Discr, Traits>>::DerivativeTemplate;

    INLINE void additionalCreate(Accumulated& results) {
        results.insert<typename Traits::Type>(Id, OrderEnum::ZERO, BufferSource::UNIQUE);
    }

    INLINE void additionalInitialize(const Storage& input, Accumulated& results) {
        v = input.getDt<Vector>(QuantityId::POSITION);
        discr.initialize(input);
        deriv = results.getBuffer<typename Traits::Type>(Id, OrderEnum::ZERO);
    }

    INLINE bool additionalEquals(const VelocityTemplate& UNUSED(other)) const {
        return true;
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        const typename Traits::Type dv = Traits::eval(v[j] - v[i], grad);
        deriv[i] += discr.eval(i, j, dv);
        if (Symmetrize) {
            deriv[j] += discr.eval(j, i, dv);
        }
    }
};

struct DivergenceTraits {
    using Type = Float;

    INLINE static Float eval(const Vector& v, const Vector& grad) {
        return dot(v, grad);
    }
};

struct RotationTraits {
    using Type = Vector;

    INLINE static Vector eval(const Vector& v, const Vector& grad) {
        // nabla x v
        return cross(grad, v);
    }
};

struct GradientTraits {
    using Type = SymmetricTensor;

    INLINE static SymmetricTensor eval(const Vector& v, const Vector& grad) {
        return symmetricOuter(v, grad);
    }
};


template <typename Discr>
using VelocityDivergence = VelocityTemplate<QuantityId::VELOCITY_DIVERGENCE, Discr, DivergenceTraits>;

template <typename Discr>
using VelocityRotation = VelocityTemplate<QuantityId::VELOCITY_ROTATION, Discr, RotationTraits>;

template <typename Discr>
using VelocityGradient = VelocityTemplate<QuantityId::VELOCITY_GRADIENT, Discr, GradientTraits>;


/// \brief Creates a given velocity derivative, using discretization given by selected SPH formulation.
///
/// Note that other formulations can still be used, provided the specialization of VelocityTemplate for given
/// discretization is defined.
template <template <class> class TVelocityDerivative>
AutoPtr<IDerivative> makeDerivative(const RunSettings& settings, Flags<DerivativeFlag> flags = EMPTY_FLAGS) {
    const DiscretizationEnum formulation =
        settings.get<DiscretizationEnum>(RunSettingsId::SPH_DISCRETIZATION);
    switch (formulation) {
    case DiscretizationEnum::STANDARD:
        return makeAuto<TVelocityDerivative<CenterDensityDiscr>>(settings, flags);
    case DiscretizationEnum::BENZ_ASPHAUG:
        return makeAuto<TVelocityDerivative<NeighborDensityDiscr>>(settings, flags);
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
