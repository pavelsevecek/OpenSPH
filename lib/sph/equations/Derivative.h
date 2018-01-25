#pragma once

/// \file Derivative.h
/// \brief Spatial derivatives to be computed by SPH discretization
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/containers/ArrayView.h"
#include "objects/finders/INeighbourFinder.h"
#include "objects/geometry/Vector.h"
#include "quantities/Storage.h"
#include "sph/equations/Accumulated.h"
#include "system/Profiler.h"
#include "system/Settings.h"
#include <set>

NAMESPACE_SPH_BEGIN


/// \brief Defines the phases of derivative evaluation.
///
/// Derivatives in different phases are evaluated in order, each phase can use results computed by derivatives
/// in previous phase. Order of derivative evaluation within one phase is undefined.
///
/// \attention Phases make only sense for asymmetric evaluation, i.e. if all neighbours are visited for given
/// particles and the computed value is final. For symmetrized evaluation, only a partial value is computed
/// for given particle (and the rest is added when evaluating the neighbouring particles), so the result from
/// previous phase cannot be used anyway!
enum class DerivativePhase {
    /// Auxiliary quantities needed for evaluation of other derivatives (grad-h, correction tensor, ...)
    PRECOMPUTE,

    /// Evaluation of quantity derivatives. All derivatives from precomputation phase are computed
    EVALUATION,
};

/// \brief Derivative accumulated by summing up neighbouring particles.
///
/// If solver is parallelized, each threa has its own derivatives that are summed after the solver loop.
/// In order to use derived classes in DerivativeHolder, they must be either default constructible or have
/// a constructor <code>Derivative(const RunSettings& settings)</code>; DerivativeHolder will construct
/// the derivative using the settings constructor if one is available.
class IDerivative : public Polymorphic {
public:
    /// \brief Returns the phase of the derivative.
    ///
    /// As long as the derivative does not depend on other derivatives, it shall return
    /// DerivativePhase::EVALUATION. Auxiliary derivatives, computing helper term that are later used for
    /// evaluation of other derivatives, belong to phase DerivativePhase::PRECOMPUTE.
    virtual DerivativePhase phase() const = 0;

    /// \brief Emplace all needed buffers into shared storage.
    ///
    /// Called only once at the beginning of the run.
    virtual void create(Accumulated& results) = 0;

    /// \brief Initialize derivative before iterating over neighbours.
    ///
    /// All pointers and arrayviews used to access storage quantities must be initialized in this function, as
    /// they may have been invalidated.
    /// \param input Storage containing all the input quantities from which derivatives are computed.
    ///              This storage is shared for all threads.
    /// \param results Thread-local storage where the computed derivatives are saved.
    virtual void initialize(const Storage& input, Accumulated& results) = 0;

    /// \brief Compute derivatives of given particle.
    ///
    /// Derivatives are computed by summing up pair-wise interaction between the given particle and its
    /// neighbours. Derivatives of the neighbours should not be modified by the function. Note that the given
    /// particle is NOT included in its own neighbours.
    /// \param idx Index of the first interacting particle
    /// \param neighs Array containing all neighbours of idx-th particle
    /// \param grads Computed gradients of the SPH kernel
    virtual void evalNeighs(const Size idx, ArrayView<const Size> neighs, ArrayView<const Vector> grads) = 0;
};

/// \brief Extension of derivative, allowing a symmetrized evaluation.
class SymmetricDerivative : public IDerivative {
public:
    /// \brief Compute a part of derivatives from interaction of particle pairs.
    ///
    /// The function computes derivatives of the given particle as well as all of its neighbours. Each
    /// particle pair is visited only once when evaluating the derivatives. Note that it computes only a part
    /// of the total sum, the rest of the sum is computed when executing the function for neighbouring
    /// particles. The derivatives are completed only after all particles have been processed.
    /// \param idx Index of first interacting particle.
    /// \param neighs Array of some neighbours of idx-th particle. May be empty.
    /// \param grads Computed gradients of the SPH kernel.
    virtual void evalSymmetric(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) = 0;

    /// For symmetric evaluation, the concept of phases does not make sense. If one needs to use auxiliary
    /// derivatives in symmetric solver, it is necessary to do two passes over all particles and evaluated
    /// different sets of derivatives in each pass.
    virtual DerivativePhase phase() const override {
        return DerivativePhase::EVALUATION;
    }
};

/// \brief Helper template for derivatives that define both the symmetrized and asymmetric variant.
template <typename TDerived>
class DerivativeTemplate : public SymmetricDerivative {
public:
    virtual void evalNeighs(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) override {
        ASSERT(neighs.size() == grads.size());
        for (Size i = 0; i < neighs.size(); ++i) {
            static_cast<TDerived*>(this)->template eval<false>(idx, neighs[i], grads[i]);
        }
    }

    virtual void evalSymmetric(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) override {
        ASSERT(neighs.size() == grads.size());
        for (Size i = 0; i < neighs.size(); ++i) {
            static_cast<TDerived*>(this)->template eval<true>(idx, neighs[i], grads[i]);
        }
    }
};

template <typename TFunctor>
using ProductType = decltype(TFunctor::product(std::declval<Vector>(), std::declval<Vector>()));

template <QuantityId Id, typename Type, typename TDerived>
class VelocityTemplateBase : public DerivativeTemplate<TDerived> {
protected:
    ArrayView<const Float> rho, m;
    ArrayView<const Vector> v;

    ArrayView<Type> deriv;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Type>(Id, OrderEnum::ZERO);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS);
        v = input.getDt<Vector>(QuantityId::POSITION);
        deriv = results.getBuffer<Type>(Id, OrderEnum::ZERO);
    }
};

template <QuantityId Id, typename TFunctor>
class VelocityTemplate
    : public VelocityTemplateBase<Id, ProductType<TFunctor>, VelocityTemplate<Id, TFunctor>> {
public:
    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        const auto dv = TFunctor::product(this->v[j] - this->v[i], grad);
        this->deriv[i] += this->m[j] / this->rho[j] * dv;
        if (Symmetrize) {
            this->deriv[j] += this->m[i] / this->rho[i] * dv;
        }
    }
};


template <QuantityId Id, typename TFunctor>
class DensityVelocityTemplate
    : public VelocityTemplateBase<Id, ProductType<TFunctor>, DensityVelocityTemplate<Id, TFunctor>> {
public:
    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        const auto rhoDv = TFunctor::product(this->v[j] - this->v[i], grad);
        this->deriv[i] += this->m[j] * rhoDv;
        if (Symmetrize) {
            this->deriv[j] += this->m[i] * rhoDv;
        }
    }
};

struct Dot {
    INLINE static Float product(const Vector& v1, const Vector& v2) {
        return dot(v1, v2);
    }
};

struct Cross {
    INLINE static Vector product(const Vector& v1, const Vector& v2) {
        return cross(v1, v2);
    }
};

struct Outer {
    INLINE static SymmetricTensor product(const Vector& v1, const Vector& v2) {
        return outer(v1, v2);
    }
};

using VelocityDivergence = VelocityTemplate<QuantityId::VELOCITY_DIVERGENCE, Dot>;
using VelocityRotation = VelocityTemplate<QuantityId::VELOCITY_ROTATION, Cross>;
using VelocityGradient = VelocityTemplate<QuantityId::VELOCITY_GRADIENT, Outer>;
using DensityVelocityDivergence = DensityVelocityTemplate<QuantityId::DENSITY_VELOCITY_DIVERGENCE, Dot>;


/// \brief Velocity gradient accumulated only over particles of the same body and only particles with
/// strength.
///
/// Here we only sum up particle belonging to the same body to avoid creating unphysical strength between
/// different bodies; a trick to add the discontinuity that doesn't exist in SPH continuum. Optionally,
/// the velocity gradient can be corrected by a auxiliary tensor; the summed gradient is  multiplied by the
/// inner product of kernel gradient and the inverse \f$C_{ij}^{-1}\f$, where \f$C_{ij}\f$ is an identity
/// matrix discretized by SPH (and thus not generally equal to identity matrix).
///
/// See paper 'Collisions between equal-sized ice grain agglomerates' by Schafer et al. (2007).
template <QuantityId Id, typename TDerived>
class StrengthVelocityTemplate : public SymmetricDerivative {
protected:
    ArrayView<const Float> rho, m;
    ArrayView<const Vector> r, v;
    ArrayView<const Size> idxs;
    ArrayView<const Float> reduce;

    ArrayView<SymmetricTensor> C;
    ArrayView<SymmetricTensor> deriv;

    bool useCorrectionTensor;

public:
    explicit StrengthVelocityTemplate(const RunSettings& settings) {
        useCorrectionTensor = settings.get<bool>(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR);
    }

    virtual void create(Accumulated& results) override {
        results.insert<SymmetricTensor>(Id, OrderEnum::ZERO);

        if (useCorrectionTensor) {
            // not really needed to save the result, using for debugging purposes
            results.insert<SymmetricTensor>(QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO);
        }
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS);
        ArrayView<const Vector> dv;
        tie(r, v, dv) = input.getAll<Vector>(QuantityId::POSITION);
        idxs = input.getValue<Size>(QuantityId::FLAG);
        reduce = input.getValue<Float>(QuantityId::STRESS_REDUCING);

        deriv = results.getBuffer<SymmetricTensor>(Id, OrderEnum::ZERO);
        if (useCorrectionTensor) {
            C = results.getBuffer<SymmetricTensor>(
                QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO);
        }
    }

    virtual void evalNeighs(const Size i,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) override {
        ASSERT(neighs.size() == grads.size());
        if (useCorrectionTensor) {
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                SymmetricTensor t = outer(r[j] - r[i], grads[k]); // symmetric in i,j ?
                C[i] += m[j] / rho[j] * t;
            }

            // sanity check that we are not getting 'weird' tensors with non-positive values on diagonal
            ASSERT(minElement(C[i].diagonal()) >= 0._f, C[i]);
            if (C[i].determinant() > 0.01_f) {
                C[i] = C[i].inverse();
                ASSERT(C[i].determinant() > 0._f, C[i]);
            } else {
                C[i] = C[i].pseudoInverse(1.e-3_f);
            }

            for (Size k = 0; k < neighs.size(); ++k) {
                const Vector correctedGrad = C[i] * grads[k];
                static_cast<TDerived*>(this)->template eval<false>(i, neighs[k], correctedGrad);
            }
        } else {
            for (Size k = 0; k < neighs.size(); ++k) {
                static_cast<TDerived*>(this)->template eval<false>(i, neighs[k], grads[k]);
            }
        }
    }

    virtual void evalSymmetric(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) override {
        ASSERT(neighs.size() == grads.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            static_cast<TDerived*>(this)->template eval<true>(idx, neighs[k], grads[k]);
        }
    }
};

class StrengthVelocityGradient
    : public StrengthVelocityTemplate<QuantityId::STRENGTH_VELOCITY_GRADIENT, StrengthVelocityGradient> {
public:
    using StrengthVelocityTemplate<QuantityId::STRENGTH_VELOCITY_GRADIENT,
        StrengthVelocityGradient>::StrengthVelocityTemplate;

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        /// \todo this is currently taken from SPH5 to allow easier comparison, drawback is that the
        /// density (and smoothing length) is evolved using this derivative or velocity divergence
        /// depending on damage status, it would be better to but this heuristics directly into the
        /// derivative, if possible
        if (idxs[i] != idxs[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
            return;
        }
        const Vector dv = v[j] - v[i];
        const SymmetricTensor t = outer(dv, grad);
        ASSERT(isReal(t));
        deriv[i] += m[j] / rho[j] * t;
        if (Symmetrize) {
            deriv[j] += m[i] / rho[i] * t;
        }
    }
};

class StrengthDensityVelocityGradient
    : public StrengthVelocityTemplate<QuantityId::STRENGTH_DENSITY_VELOCITY_GRADIENT,
          StrengthDensityVelocityGradient> {
public:
    using StrengthVelocityTemplate<QuantityId::STRENGTH_DENSITY_VELOCITY_GRADIENT,
        StrengthDensityVelocityGradient>::StrengthVelocityTemplate;

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        if (idxs[i] != idxs[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
            return;
        }
        const Vector dv = v[j] - v[i];
        const SymmetricTensor t = outer(dv, grad);
        ASSERT(isReal(t));
        deriv[i] += m[j] * t;
        if (Symmetrize) {
            deriv[j] += m[i] * t;
        }
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
protected:
    Accumulated accumulated;

    struct Less {
        bool operator()(const AutoPtr<IDerivative>& d1, const AutoPtr<IDerivative>& d2) {
            if (d1->phase() != d2->phase()) {
                // sort primarily by phases
                return d1->phase() < d2->phase();
            }
            // used sorting of pointers for order within the same phase
            return &*d1 < &*d2;
        }
    };

    /// \brief Holds all derivatives that are evaluated in the loop.
    ///
    /// Modules save data to the \ref accumulated storage; one module can use multiple buffers (acceleration
    /// and energy derivative) and multiple modules can write into same buffer (different terms in equation of
    /// motion). Modules are evaluated consecutively (within one thread), so this is thread-safe.
    std::set<AutoPtr<IDerivative>, Less> derivatives;

public:
    /// \brief Adds derivative if not already present.
    ///
    /// If the derivative is already stored, new one is NOT created, even if settings are different.
    template <typename Type>
    void require(const RunSettings& settings) {
        for (const AutoPtr<IDerivative>& d : derivatives) {
            const IDerivative& value = *d;
            if (typeid(value) == typeid(Type)) {
                return;
            }
        }
        AutoPtr<Type> ptr = makeAuto<Type>(makeFromSettings<Type>(settings));
        derivatives.insert(std::move(ptr));
    }

    /// Initialize derivatives before loop
    void initialize(const Storage& input) {
        if (accumulated.getBufferCnt() == 0) {
            //   PROFILE_SCOPE("DerivativeHolder create");
            // lazy buffer creation
            /// \todo this will be called every time if derivatives do not create any buffers,
            /// does it really matter?
            for (const auto& deriv : derivatives) {
                deriv->create(accumulated);
            }
        }
        accumulated.initialize(input.getParticleCnt());
        // initialize buffers first, possibly resizing then and invalidating previously stored arrayviews

        // PROFILE_SCOPE("DerivativeHolder initialize accumulated");
        for (const auto& deriv : derivatives) {
            //     PROFILE_SCOPE("DerivativeHolder initialize derivatvies");
            // then get the arrayviews for derivatives
            deriv->initialize(input, accumulated);
        }
    }

    void eval(const Size idx, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        ASSERT(neighs.size() == grads.size());
        for (const auto& deriv : derivatives) {
            deriv->evalNeighs(idx, neighs, grads);
        }
    }

    void evalSymmetric(const Size idx, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        ASSERT(neighs.size() == grads.size());
        for (const auto& deriv : derivatives) {
            SymmetricDerivative* symmetric = assert_cast<SymmetricDerivative*>(&*deriv);
            symmetric->evalSymmetric(idx, neighs, grads);
        }
    }

    INLINE Accumulated& getAccumulated() {
        return accumulated;
    }

    INLINE Size getDerivativeCnt() const {
        return derivatives.size();
    }

    INLINE bool isSymmetric() const {
        for (const auto& deriv : derivatives) {
            if (!dynamic_cast<SymmetricDerivative*>(&*deriv)) {
                return false;
            }
        }
        return true;
    }
};

NAMESPACE_SPH_END
