#pragma once

/// \file GradHSolver.h
/// \brief Extension of SPH solver taking into account the gradient of smoothing lengths
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "sph/solvers/AsymmetricSolver.h"

NAMESPACE_SPH_BEGIN

/// \brief Special derivative evaluated by \ref GradHSolver
///
/// Unlike other derivatives which use the same gradient for both particles in evaluated pair (due to
/// symmetrization in smoothing lenghts or direct symmetrization of kernel values), asymmetric derivatives
/// have generally different gradients for both particles.
class IAsymmetricDerivative : public IDerivative {
public:
    /// \brief Compute a part of derivatives from interaction of particle pairs.
    ///
    /// \param idx Index of first interacting particle.
    /// \param neighs Array of some neighbours of idx-th particle. May be empty.
    /// \param gradi Computed gradients of the SPH kernel for particle i.
    /// \param gradj Computed gradients of the SPH kernel for particle j.
    virtual void evalAsymmetric(const Size idx,
        ArrayView<const Size> neighs,
        ArrayView<const Vector> gradi,
        ArrayView<const Vector> gradj) = 0;
};

/// \todo
class IAsymmetricTerm : public Polymorphic {
public:
    virtual void setAsymmetricDerivatives(Array<AutoPtr<IAsymmetricDerivative>>& asymmetricDerivatives) = 0;
};

// class EquationHolder : public EquationHolder3


class GradHSolver : public AsymmetricSolver {
private:
    Array<AutoPtr<IAsymmetricDerivative>> asymmetricDerivatives;

    /// Thread-local data for the second particle in the particle pair
    struct SecondThreadData {
        Array<Vector> grads;
    };

    ThreadLocal<SecondThreadData> secondData;

public:
    GradHSolver(IScheduler& scheduler,
        const RunSettings& settings,
        const EquationHolder& basicTerms,
        Array<AutoPtr<IAsymmetricTerm>>&& asymmetricTerms);

    virtual void create(Storage& storage, IMaterial& material) const override;

protected:
    virtual void loop(Storage& storage, Statistics& stats) override;
};


NAMESPACE_SPH_END
