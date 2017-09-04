#pragma once

/// \file Derivative.h
/// \brief Spatial derivatives to be computed by SPH discretization
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/containers/ArrayView.h"
#include "objects/finders/INeighbourFinder.h"
#include "objects/geometry/Vector.h"
#include "quantities/Storage.h"
#include "sph/equations/Accumulated.h"
#include "system/Profiler.h"
#include "system/Settings.h"
#include <map>

NAMESPACE_SPH_BEGIN

/// \brief Derivative accumulated by summing up neighbouring particles.
///
/// If solver is parallelized, each threa has its own derivatives that are summed after the solver loop.
/// In order to use derived classes in DerivativeHolder, they must be either default constructible or have
/// a constructor <code>Derivative(const RunSettings& settings)</code>; DerivativeHolder will construct
/// the derivative using the settings constructor if one is available.
class IDerivative : public Polymorphic {
public:
    /// Emplace all needed buffers into shared storage. Called only once at the beginning of the run.
    virtual void create(Accumulated& results) = 0;

    /// Initialize derivative before iterating over neighbours.
    /// \param input Storage containing all the input quantities from which derivatives are computed.
    ///              This storage is shared for all threads.
    /// \param results Thread-local storage where the computed derivatives are saved.
    virtual void initialize(const Storage& input, Accumulated& results) = 0;

    /// Compute derivatives from interaction of particle pairs. Each particle pair is visited only once,
    /// the derivatives shall be computed for both particles.
    /// \param idx Index of first interacting particle
    /// \param neighs Array of neighbours of idx-th particle
    /// \param grads Computed gradients of the SPH kernel
    virtual void evalSymmetric(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) = 0;

    /// Compute derivatives from interaction of particle pairs. Only compute derivatives of particle i,
    /// the function is called for every particle.
    /// \param idx Index of first interacting particle
    /// \param neighs Array of neighbours of idx-th particle
    /// \param grads Computed gradients of the SPH kernel
    virtual void evalAsymmetric(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) = 0;
};

/// \brief Helper functor returning the input gradient without any modification.
///
/// This is the default template parameter for velocity derivative modifiers. Alternatively, a
/// \ref AngularMomentumCorrection can be used, modifying the gradient in order to improve conservation of the
/// total angular momentum.
struct NoGradientCorrection {
    /// \brief Returns the modified gradient.
    INLINE Vector operator()(const Size UNUSED(i), const Vector& grad) {
        return grad;
    }

    /// Initialize the correction.
    void initialize(const Storage& UNUSED(input)) {}
};

template <typename TDerived>
class DerivativeTemplate : public IDerivative {
public:
    virtual void evalSymmetric(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) override {
        static_cast<TDerived*>(this)->template eval<true>(idx, neighs, grads);
    }

    virtual void evalAsymmetric(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) override {
        static_cast<TDerived*>(this)->template eval<false>(idx, neighs, grads);
    }
};

namespace Detail {
    template <typename Type, QuantityId Id, typename TCorrection, typename TDerived>
    class VelocityTemplate : public DerivativeTemplate<VelocityTemplate<Type, Id, TCorrection, TDerived>> {
    private:
        ArrayView<const Float> rho, m;
        ArrayView<const Vector> v;
        ArrayView<Type> deriv;
        TCorrection correction;

    public:
        virtual void create(Accumulated& results) override {
            results.insert<Type>(Id, OrderEnum::ZERO);
        }

        virtual void initialize(const Storage& input, Accumulated& results) override {
            tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASSES);
            v = input.getDt<Vector>(QuantityId::POSITIONS);
            deriv = results.getBuffer<Type>(Id, OrderEnum::ZERO);

            correction.initialize(input);
        }

        template <bool Symmetrize>
        INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
            ASSERT(neighs.size() == grads.size());
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];

                if (std::is_same<TCorrection, NoGradientCorrection>::value) {
                    const auto dv = TDerived::func(v[j] - v[i], grads[k]);
                    deriv[i] += m[j] / rho[j] * dv;
                    if (Symmetrize) {
                        deriv[j] += m[i] / rho[i] * dv;
                    }
                } else {
                    deriv[i] += m[j] / rho[j] * TDerived::func(v[j] - v[i], correction(i, grads[k]));
                    if (Symmetrize) {
                        deriv[j] += m[i] / rho[i] * TDerived::func(v[j] - v[i], correction(j, grads[k]));
                    }
                }
            }
        }
    };
}

template <typename TCorrection>
struct VelocityDivergence : public Detail::VelocityTemplate<Float,
                                QuantityId::VELOCITY_DIVERGENCE,
                                TCorrection,
                                VelocityDivergence<TCorrection>> {
    INLINE static Float func(const Vector& dv, const Vector& grad) {
        return dot(dv, grad);
    }
};

template <typename TCorrection>
struct VelocityGradient : public Detail::VelocityTemplate<SymmetricTensor,
                              QuantityId::VELOCITY_GRADIENT,
                              TCorrection,
                              VelocityGradient<TCorrection>> {
    INLINE static SymmetricTensor func(const Vector& dv, const Vector& grad) {
        return outer(dv, grad);
    }
};

template <typename TCorrection>
struct VelocityRotation : public Detail::VelocityTemplate<Vector,
                              QuantityId::VELOCITY_ROTATION,
                              TCorrection,
                              VelocityRotation<TCorrection>> {
    INLINE static Vector func(const Vector& dv, const Vector& grad) {
        return cross(dv, grad);
    }
};

/// Velocity gradient accumulated only over particles of the same body and only particles with strength.
template <typename TCorrection>
class StrengthVelocityGradient : public DerivativeTemplate<StrengthVelocityGradient<TCorrection>> {
private:
    ArrayView<const Float> rho, m;
    ArrayView<const Vector> v;
    ArrayView<const Size> idxs;
    ArrayView<const Float> reduce;
    ArrayView<SymmetricTensor> deriv;
    TCorrection correction;

public:
    virtual void create(Accumulated& results) override {
        results.insert<SymmetricTensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT, OrderEnum::ZERO);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASSES);
        v = input.getDt<Vector>(QuantityId::POSITIONS);
        idxs = input.getValue<Size>(QuantityId::FLAG);
        reduce = input.getValue<Float>(QuantityId::STRESS_REDUCING);

        deriv = results.getBuffer<SymmetricTensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT, OrderEnum::ZERO);

        correction.initialize(input);
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        ASSERT(neighs.size() == grads.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            /// \todo this is currently taken from SPH5 to allow easier comparison, drawback is that the
            /// density (and smoothing length) is evolved using this derivative or velocity divergence
            /// depending on damage status, it would be better to but this heuristics directly into the
            /// derivative, if possible
            if (idxs[i] != idxs[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
                continue;
            }
            const Vector dv = v[j] - v[i];
            if (std::is_same<TCorrection, NoGradientCorrection>::value) {
                // optimization, avoid computing outer product twice
                const SymmetricTensor t = outer(dv, grads[k]);
                ASSERT(isReal(t));
                deriv[i] += m[j] / rho[j] * t;
                if (Symmetrize) {
                    deriv[j] += m[i] / rho[i] * t;
                }
            } else {
                deriv[i] += m[j] / rho[j] * outer(dv, correction(i, grads[k]));
                if (Symmetrize) {
                    deriv[j] += m[i] / rho[i] * outer(dv, correction(j, grads[k]));
                }
            }
        }
    }
};

/// \brief Correction tensor to improve conservation of total angular momentum.
///
/// This is only useful when stress tensor is used in equation of motion, as total angular momentum is
/// conversed by SPH discretization when only pressure gradient is used.
///
/// The computed tensor \f$C_{ij}\f$ is used to modify the total velocity gradient (and subsequently
/// divergence and rotation): the summed velocity divergences are multiplied by the inner product of kernel
/// gradient and the inverse \f$C_{ij}^{-1}\f$. This means that for self-consistent solution we need to do two
/// passes over particle pairs. However, this is only a numerical correction anyway, so we can use values from
/// previous timestep, like we do for any time-dependent quantities.
///
/// See paper 'Collisions between equal-sized ice grain agglomerates' by Schafer et al. (2007).
class AngularMomentumCorrectionTensor : public DerivativeTemplate<AngularMomentumCorrectionTensor> {
private:
    ArrayView<const Float> m;
    ArrayView<const Float> rho;
    ArrayView<const Vector> r;
    ArrayView<SymmetricTensor> C;

public:
    virtual void create(Accumulated& results) override {
        results.insert<SymmetricTensor>(QuantityId::ANGULAR_MOMENTUM_CORRECTION, OrderEnum::ZERO);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(m, rho) = input.getValues<Float>(QuantityId::MASSES, QuantityId::DENSITY);
        r = input.getValue<Vector>(QuantityId::POSITIONS);
        C = results.getBuffer<SymmetricTensor>(QuantityId::ANGULAR_MOMENTUM_CORRECTION, OrderEnum::ZERO);
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        ASSERT(neighs.size() == grads.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            SymmetricTensor t = outer(r[j] - r[i], grads[k]); // symmetric in i,j ?
            C[i] += m[j] / rho[j] * t;
            if (Symmetrize) {
                C[j] += m[i] / rho[i] * t;
            }
        }
    }
};

/// \brief Velocity derivative correction conserving the total angular momentum.
class AngularMomentumCorrection {
private:
    ArrayView<const SymmetricTensor> C_inv;

public:
    INLINE Vector operator()(const Size i, const Vector& grad) {
        return C_inv[i] * grad;
    }

    void initialize(const Storage& input) {
        C_inv = input.getValue<SymmetricTensor>(QuantityId::ANGULAR_MOMENTUM_CORRECTION);
    }
};


/// \brief Helper function allowing to construct an object from settings if the object defines such
/// constructor, or default-construct the object otherwise.
template <typename Type>
std::enable_if_t<!std::is_constructible<Type, const RunSettings&>::value, Type> makeFromSettings(
    const RunSettings& UNUSED(settings)) {
    // default constructible
    return Type();
}

/// Specialization for types constructible from settings
template <typename Type>
std::enable_if_t<std::is_constructible<Type, const RunSettings&>::value, Type> makeFromSettings(
    const RunSettings& settings) {
    // constructible from settings
    return Type(settings);
}

class DerivativeHolder {
private:
    Accumulated accumulated;

    /// Holds all modules that are evaluated in the loop. Modules save data to the \ref accumulated
    /// storage; one module can use multiple buffers (acceleration and energy derivative) and multiple
    /// modules can write into same buffer (different terms in equation of motion).
    /// Modules are evaluated consecutively (within one thread), so this is thread-safe.
    Array<AutoPtr<IDerivative>> derivatives;

public:
    /// Adds derivative if not already present. If the derivative is already stored, new one is NOT
    /// created, even if settings are different.
    template <typename TDerivative>
    void require(const RunSettings& settings) {
        for (AutoPtr<IDerivative>& d : derivatives) {
            IDerivative& value = *d;
            if (typeid(value) == typeid(TDerivative)) {
                return;
            }
        }
        AutoPtr<TDerivative> ptr = makeAuto<TDerivative>(makeFromSettings<TDerivative>(settings));
        derivatives.push(std::move(ptr));
    }

    /// Initialize derivatives before loop
    void initialize(const Storage& input) {
        if (accumulated.getBufferCnt() == 0) {
            //   PROFILE_SCOPE("DerivativeHolder create");
            // lazy buffer creation
            /// \todo this will be called every time if derivatives do not create any buffers,
            /// does it really matter?
            for (auto& deriv : derivatives) {
                deriv->create(accumulated);
            }
        }
        accumulated.initialize(input.getParticleCnt());
        // initialize buffers first, possibly resizing then and invalidating previously stored arrayviews

        // PROFILE_SCOPE("DerivativeHolder initialize accumulated");
        for (auto& deriv : derivatives) {
            //     PROFILE_SCOPE("DerivativeHolder initialize derivatvies");
            // then get the arrayviews for derivatives
            deriv->initialize(input, accumulated);
        }
    }

    template <bool Symmetrize>
    void eval(const Size idx, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        ASSERT(neighs.size() == grads.size());
        for (auto& deriv : derivatives) {
            if (Symmetrize) {
                deriv->evalSymmetric(idx, neighs, grads);
            } else {
                deriv->evalAsymmetric(idx, neighs, grads);
            }
        }
    }

    INLINE Accumulated& getAccumulated() {
        return accumulated;
    }

    INLINE Size getDerivativeCnt() const {
        return derivatives.size();
    }
};

NAMESPACE_SPH_END
