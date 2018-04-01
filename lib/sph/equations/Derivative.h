#pragma once

/// \file Derivative.h
/// \brief Spatial derivatives to be computed by SPH discretization
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/containers/FlatSet.h"
#include "objects/geometry/Vector.h"
#include "quantities/Storage.h"
#include "sph/equations/Accumulated.h"
#include "system/Profiler.h"
#include "system/Settings.h"

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

    /// \brief Returns the phase of the derivative.
    ///
    /// As long as the derivative does not depend on other derivatives, it shall return
    /// DerivativePhase::EVALUATION. Auxiliary derivatives, computing helper term that are later used for
    /// evaluation of other derivatives, belong to phase DerivativePhase::PRECOMPUTE.
    virtual DerivativePhase phase() const = 0;

    /// \brief Returns true if this derivative is equal to the given derivative.
    ///
    /// This is always false if the derivative is a different type, but a derivative may also have an inner
    /// state and result in different value, even if the type is the same. This function is only used to check
    /// that we do not try to add two derivatives of the same type, but with different internal state.
    virtual bool equals(const IDerivative& other) const {
        return typeid(*this) == typeid(other);
    }
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
    virtual DerivativePhase phase() const final {
        return DerivativePhase::EVALUATION;
    }
};

enum class DerivativeFlag {
    /// Use correction tensor on kernel gradient when evaluating the derivative. Can be currently only used
    /// for asymmetric derivatives. Implies SUM_ONLY_UNDAMAGED - the correction tensor is only computed for
    /// undamaged particles.
    CORRECTED = 1 << 0,

    /// Only undamaged particles (particles with damage > 0) from the same body (body with the same flag) will
    /// contribute to the sum.
    SUM_ONLY_UNDAMAGED = 1 << 1,
};

/// \brief Helper template for derivatives that define both the symmetrized and asymmetric variant.
///
/// Derived class must implement \a eval templated member function, taking the indices of the particles and
/// the gradient as arguments. The template parameter is a bool, where false means asymmetric evaluation (only
/// the first particle should be modified), and true means symmetric evaluation. It must also implement
/// function \a init, having the same signature as \a initialize of \ref IDerivative; it initializes
/// additional ArrayViews, specific for the derived type, without the need to override the \ref initialize
/// function and manually call the function of a parent (which is why the function is marked as final).
template <typename TDerived>
class DerivativeTemplate : public SymmetricDerivative {
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
    }

    virtual void initialize(const Storage& input, Accumulated& results) final {
        if (flags.has(DerivativeFlag::CORRECTED)) {
            C = results.getBuffer<SymmetricTensor>(
                QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO);
        } else {
            C = nullptr;
        }
        if (flags.has(DerivativeFlag::SUM_ONLY_UNDAMAGED)) {
            idxs = input.getValue<Size>(QuantityId::FLAG);
            reduce = input.getValue<Float>(QuantityId::STRESS_REDUCING);
        } else {
            idxs = nullptr;
            reduce = nullptr;
        }

        derived()->init(input, results);
    }

    virtual void evalNeighs(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) override {
        ASSERT(neighs.size() == grads.size());
        if (C) {
            sum(idx, neighs, grads, [this](Size i, Size j, const Vector& grad) INL {
                ASSERT(C[i] != SymmetricTensor::null());
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
        ASSERT(neighs.size() == grads.size());
        ASSERT(!flags.has(DerivativeFlag::CORRECTED));
        sum(idx, neighs, grads, [this](Size i, Size j, const Vector& grad) INL {
            derived()->template eval<true>(i, j, grad);
        });
    }

    virtual bool equals(const IDerivative& other) const override {
        if (typeid(*this) != typeid(other)) {
            return false;
        }
        const TDerived* actOther = assert_cast<const TDerived*>(&other);
        return flags == actOther->flags;
    }

private:
    template <typename TFunctor>
    INLINE void sum(const Size i,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads,
        const TFunctor& functor) {
        if (flags.has(DerivativeFlag::SUM_ONLY_UNDAMAGED)) {
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
};

class CorrectionTensor final : public IDerivative {
private:
    ArrayView<const Vector> r;
    ArrayView<const Size> idxs;
    ArrayView<const Float> reduce;
    ArrayView<const Float> m, rho;
    ArrayView<SymmetricTensor> C;

public:
    virtual void create(Accumulated& results) override;

    virtual void initialize(const Storage& input, Accumulated& results) override;

    virtual void evalNeighs(Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) override;

    virtual DerivativePhase phase() const override;
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
class NeighbourDensityDiscr {
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

    virtual void create(Accumulated& results) override {
        results.insert<typename Traits::Type>(Id, OrderEnum::ZERO, BufferSource::UNIQUE);
    }

    INLINE void init(const Storage& input, Accumulated& results) {
        v = input.getDt<Vector>(QuantityId::POSITION);
        discr.initialize(input);
        deriv = results.getBuffer<typename Traits::Type>(Id, OrderEnum::ZERO);
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
        return outer(v, grad);
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
    const FormulationEnum formulation = settings.get<FormulationEnum>(RunSettingsId::SPH_FORMULATION);
    switch (formulation) {
    case FormulationEnum::STANDARD:
        return makeAuto<TVelocityDerivative<CenterDensityDiscr>>(settings, flags);
    case FormulationEnum::BENZ_ASPHAUG:
        return makeAuto<TVelocityDerivative<NeighbourDensityDiscr>>(settings, flags);
    default:
        NOT_IMPLEMENTED;
    }
}


class DerivativeHolder {
private:
    Accumulated accumulated;

    struct Less {
        bool operator()(const AutoPtr<IDerivative>& d1, const AutoPtr<IDerivative>& d2) const {
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
    /// Modules save data to the \ref accumulated storage; one module can use multiple buffers
    /// (acceleration and energy derivative) and multiple modules can write into same buffer (different
    /// terms in equation of motion). Modules are evaluated consecutively (within one thread), so this is
    /// thread-safe.
    FlatSet<AutoPtr<IDerivative>, Less> derivatives;

    /// Used for lazy creation of derivatives
    bool needsCreate = true;

public:
    /// \brief Adds derivative if not already present.
    ///
    /// If the derivative is already stored, new one is NOT stored, it is simply ignored. However, the new
    /// derivative must be equal to the one already stored, which is checked using \ref IDerivative::equals.
    /// If derivative has the same type, but different internal state, exception InvalidState is thrown.
    void require(AutoPtr<IDerivative>&& derivative);

    /// \brief Initialize derivatives before loop
    void initialize(const Storage& input);

    /// \brief Evaluates all held derivatives for given particle.
    void eval(const Size idx, ArrayView<const Size> neighs, ArrayView<const Vector> grads);

    /// \brief Evaluates all held derivatives symetrically for given particle pairs.
    ///
    /// Useful to limit the total number of evaluations - every particle pair can be evaluated only once.
    void evalSymmetric(const Size idx, ArrayView<const Size> neighs, ArrayView<const Vector> grads);

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
