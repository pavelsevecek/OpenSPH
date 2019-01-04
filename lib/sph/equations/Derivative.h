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

    /// Evaluation of quantity derivatives. All derivatives from precomputation phase are already computed and
    /// may be used for the computation here.
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
    /// \param idx Index of the first interacting particle.
    /// \param neighs Array containing all neighbours of idx-th particle.
    /// \param grads Computed gradients of the SPH kernel for each particle pair.
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
///
/// User-defined derivatives must be derived from this class if \ref SymmetricSolver is used in the
/// simulation. For \ref AsymmetricSolver, it is sufficient to implement the \ref IDerivative.
class ISymmetricDerivative : public IDerivative {
public:
    /// \brief Compute a part of derivatives from interaction of particle pairs.
    ///
    /// The function computes derivatives of the given particle as well as all of its neighbours. Each
    /// particle pair is visited only once when evaluating the derivatives. Note that it computes only a part
    /// of the total sum, the rest of the sum is computed when executing the function for neighbouring
    /// particles. The derivatives are completed only after all particles have been processed.
    ///
    /// Implementation must be consistent with \ref evalNeighs, both must evaluate to the same derivative
    /// values. To ensure the derivative is internally consistent, consider using \ref DerivativeTemplate
    /// helper class.
    ///
    /// \param idx Index of first interacting particle.
    /// \param neighs Array of some neighbours of idx-th particle. May be empty.
    /// \param grads Computed gradients of the SPH kernel for each particle pair.
    virtual void evalSymmetric(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads) = 0;

    /// \brief Symmetric derivatives are always in \ref EVALUATION phase.
    ///
    /// For symmetric evaluation, the concept of phases does not make sense. If one needs to use auxiliary
    /// derivatives in symmetric solver, it is necessary to do two passes over all particles and evaluated
    /// different sets of derivatives in each pass.
    virtual DerivativePhase phase() const final {
        return DerivativePhase::EVALUATION;
    }
};

/// \brief Extension of derivative allowing to compute pair-wise acceleration for each neighbour.
///
/// Any derivative modifying acceleration of particles must implement this interface in order to work properly
/// with \ref EnergyConservingSolver. If different solver is used, it is sufficient the base classes.
class IAcceleration : public ISymmetricDerivative {
public:
    /// \brief Computes the pair-wise accelerations of given particle and its neighbours.
    ///
    /// If the implementation of \ref evalNeighs and \ref evalNeighs also modify other quantity derivatives
    /// besides accelerations, this function shall also modify these in order to be consistent. Here, the
    /// evaluation is asymmetric, so the call of \ref evalAcceleration should have the same effect as \ref
    /// evalNeighs for all quantities except accelerations and energy derivative. Implementation may (but does
    /// not have to) modify energy derivatives arbitrarily; they are later overriden by \ref
    /// EnergyConservingSolver anyway.
    ///
    /// To ensure the computation of accelerations is internally consistent with other methods of derivative
    /// evaluation, consider using \ref AccelerationTemplate helper class.
    ///
    /// \param idx Index of first interacting particle.
    /// \param neighs Array containing all neighbours of idx-th particle.
    /// \param grads Computed gradients of the SPH kernel for each particle pair.
    /// \param dv Output view, where the pair-wise accelerations are stored.
    virtual void evalAcceleration(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> grads,
        ArrayView<Vector> dv) = 0;
};


class CorrectionTensor final : public IDerivative {
private:
    ArrayView<const Vector> r;
    ArrayView<const Size> idxs;
    ArrayView<const Float> reduce;
    ArrayView<const Float> m, rho;
    ArrayView<SymmetricTensor> C;

    bool sumOnlyUndamaged;

public:
    explicit CorrectionTensor(const RunSettings& settings);

    virtual void create(Accumulated& results) override;

    virtual void initialize(const Storage& input, Accumulated& results) override;

    virtual void evalNeighs(Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) override;

    virtual DerivativePhase phase() const override;
};


/// \brief Container holding derivatives and the storage they accumulate to.
class DerivativeHolder : public Polymorphic {
private:
    Accumulated accumulated;

    struct Less {
        INLINE bool operator()(const AutoPtr<IDerivative>& d1, const AutoPtr<IDerivative>& d2) const {
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
    virtual void require(AutoPtr<IDerivative>&& derivative);

    /// \brief Initialize derivatives before loop.
    virtual void initialize(const Storage& input);

    /// \brief Evaluates all held derivatives for given particle.
    void eval(const Size idx, ArrayView<const Size> neighs, ArrayView<const Vector> grads);

    /// \brief Evaluates all held derivatives symetrically for given particle pairs.
    ///
    /// Useful to limit the total number of evaluations - every particle pair can be evaluated only once.
    /// It can be used only if all the stored derivatives are symmetric, i.e. they implement the \ref
    /// SymmetricDerivative interface.
    void evalSymmetric(const Size idx, ArrayView<const Size> neighs, ArrayView<const Vector> grads);

    /// \brief Returns true if all stored derivatives are symmetric.
    ///
    /// Only if all derivatives are symmetric, the symmetric evaluation (using \ref evalSymmetric) can be
    /// used, otherwise an assert is issued.
    bool isSymmetric() const;

    INLINE Accumulated& getAccumulated() {
        return accumulated;
    }

    INLINE Size getDerivativeCnt() const {
        return derivatives.size();
    }
};

NAMESPACE_SPH_END
