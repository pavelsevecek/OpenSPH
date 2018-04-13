#pragma once

/// \file EquationTerm.h
/// \brief Right-hand side term of SPH equations
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/wrappers/SharedPtr.h"
#include "physics/Constants.h"
#include "sph/Materials.h"
#include "sph/equations/Derivative.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

/// \brief Represents a term or terms appearing in evolutionary equations.
///
/// Each EquationTerm either directly modifies quantities, or adds quantity derivatives. These terms never
/// work directly with pairs of particles, instead they can register IDerivative that will be
/// accumulated by the solver. EquationTerm can then access the result.
class IEquationTerm : public Polymorphic {
public:
    /// \brief Sets derivatives required by this term.
    ///
    /// The derivatives are then automatically evaluated by the solver, the equation term can access the
    /// result in \ref finalize function. This function is called once for each thread at the beginning of the
    /// run.
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) = 0;

    /// \brief Initialize all the derivatives and/or quantity values before derivatives are computed.
    ///
    /// Called at the beginning of every time step. Note that derivatives need not be zeroed out manually,
    /// this is already done by timestepping (for derivatives of quantities) and solver (for accumulated
    /// values).
    virtual void initialize(Storage& storage, ThreadPool& pool) = 0;

    /// \brief Computes all the derivatives and/or quantity values based on accumulated derivatives.
    ///
    /// Called every time step after derivatives are evaluated and saved to storage.
    virtual void finalize(Storage& storage, ThreadPool& pool) = 0;

    /// \brief Creates all quantities needed by the term using given material.
    ///
    /// Called once for every body in the simulation.
    virtual void create(Storage& storage, IMaterial& material) const = 0;
};


/// \brief Discretization of pressure term in standard SPH formulation.
class StandardForceDiscr {
    ArrayView<const Float> rho;

public:
    void initialize(const Storage& input) {
        rho = input.getValue<Float>(QuantityId::DENSITY);
    }

    template <typename T>
    INLINE T eval(const Size i, const Size j, const T& vi, const T& vj) {
        return vi / sqr(rho[i]) + vj / sqr(rho[j]);
    }
};

/// \brief Discretization of pressure term in code SPH5
class BenzAsphaugForceDiscr {
    ArrayView<const Float> rho;

public:
    void initialize(const Storage& input) {
        rho = input.getValue<Float>(QuantityId::DENSITY);
    }

    template <typename T>
    INLINE T eval(const Size i, const Size j, const T& vi, const T& vj) {
        return (vi + vj) / (rho[i] * rho[j]);
    }
};

/// \brief Evolutionary equation for the (scalar) smoothing length
///
/// The smoothing length is evolved using first-order equation, 'mirroring' changes in density in
/// order to keep the particle concentration approximately constant. The derivative of smoothing
/// length \f$h\f$ of i-th particle computed using: \f[
///  \frac{{\rm d} h_i}{{\rm d} t} = \frac{h_i}{D} \nabla \cdot \vec v_i \,,
/// \f]
/// where \f$D\f$ is the number of spatial dimensions (3 unless specified otherwise) and \f$\vec
/// v_i\f$ is the velocity of the particle.
///
/// It is possible to add additional term into the equation that increases/decreases the smoothing
/// length when number of neighbours is too low/high. This is done by setting flag
/// SmoothingLengthEnum::SOUND_SPEED_ENFORCING to settings entry
/// RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH. The allowed range of neighbours is then controlled
/// by RunSettingsId::SPH_NEIGHBOUR_RANGE. Note that the number of neighbours is not guaranteed to
/// be in the given range, the actual number of neighbours cannot be precisely controlled.
class AdaptiveSmoothingLength : public IEquationTerm {
protected:
    /// Number of spatial dimensions of the simulation. This should match the dimensions of the SPH kernel.
    Size dimensions;

    /// Minimal allowed value of the smoothing length
    Float minimal;

    struct {
        Float strength;
        Interval range;
    } enforcing;

public:
    explicit AdaptiveSmoothingLength(const RunSettings& settings, const Size dimensions = DIMENSIONS);

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override;

    virtual void initialize(Storage& storage, ThreadPool& pool) override;

    virtual void finalize(Storage& storage, ThreadPool& pool) override;

    virtual void create(Storage& storage, IMaterial& material) const override;

private:
    /// \brief Modifies the smoothing length derivative, attempting to enforce that the number of neighbours
    /// is within the given interval.
    ///
    /// Note that is still may not be possible to guarantee the given number of neighbours; for example if
    /// particles are in a regular grid, only certain (discrete) numbers of neighbours are possible.
    /// \param i Index of particle to modify
    /// \param v View of the particle velocities
    /// \param cs View of sound speed, used to non-dimensionalize the expression.
    /// \param neighCnt Current number of particle neighbours
    void enforce(const Size i,
        ArrayView<Vector> v,
        ArrayView<const Float> cs,
        ArrayView<const Size> neighCnt);
};


/// \brief Equation of motion due to pressure gradient
///
/// Computes acceleration from pressure gradient and corresponding derivative of internal energy.
/// The acceleration is given by: \f[
///  \frac{{\rm d} \vec v_i}{{\rm d} t} = \sum_j m_j \left(\frac{p_i}{\rho_i^2} +
///  \frac{p_j}{\rho_j^2}\right) \cdot \nabla W_{ij}\,,
/// \f]
class PressureForce : public IEquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override;

    virtual void initialize(Storage& storage, ThreadPool& pool) override;

    virtual void finalize(Storage& storage, ThreadPool& pool) override;

    virtual void create(Storage& storage, IMaterial& material) const override;
};

/// \brief Equation of motion for solid body and constitutive equation for the stress tensor
/// (Hooke's law)
///
/// The equation computes acceleration from the divergence of the deviatoric stress \f$S\f$. The
/// equation of motion in the used SPH discretization reads: \f[
///  \frac{{\rm d} \vec v_i}{{\rm d} t} = \sum_j m_j \left(\frac{S_i + S_j}{\rho_i \rho_j}\right)
///  \cdot \nabla W_{ij}\,,
/// \f]
/// Corresponding term of energy equation (viscous heating) is also added to the energy
/// derivative: \f[
///  \frac{{\rm d} u_i}{{\rm d} t} = \frac{1}{\rho_i} S_i : \nabla \vec v_i \,,
/// \f]
/// where \f$\nabla \vec v_i\f$ is the symmetrized velocity gradient.
///
/// The stress is evolved as a first-order quantity, using Hooke's law as constitutive equation:
/// \f[
///  \frac{{\rm d} S_i}{{\rm d} t} = 2\mu \left( \nabla \vec v - {\bf 1} \, \frac{\nabla \cdot
///  \vec v}{3} \right)\,,
/// \f]
/// where \f$\mu\f$ is the shear modulus and \f$\bf 1\f$ is the identity tensor.
///
/// This equation represents solid bodies, for fluids use \ref NavierStokesForce.
///
/// \attention The isotropic part of the force is NOT computed, it is necessary to use \ref
/// PressureForce together with this equation.
class SolidStressForce : public IEquationTerm {
private:
    bool useCorrectionTensor;

public:
    explicit SolidStressForce(const RunSettings& settings);

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override;

    virtual void initialize(Storage& storage, ThreadPool& pool) override;

    virtual void finalize(Storage& storage, ThreadPool& pool) override;

    virtual void create(Storage& storage, IMaterial& material) const override;
};

/// \brief Navier-Stokes equation of motion
///
/// \todo
class NavierStokesForce : public IEquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override;

    virtual void initialize(Storage& storage, ThreadPool& pool) override;

    virtual void finalize(Storage& storage, ThreadPool& pool) override;

    virtual void create(Storage& storage, IMaterial& material) const override;
};

/// \brief Equation for evolution of density
///
/// Provides a first-order evolutionary equation for density, computing the derivative of i-th
/// particle using: \f[
///  \frac{{\rm d} \rho_i}{{\rm d} t} = -\rho_i \nabla \cdot \vec v_i \,,
/// \f]
/// where \f$\vec v_i\f$ is the velocity of the particle.
///
/// The equation has two modes - fluid and solid - differing in the particle set used in the sum
/// of velocity divergence. The modes are specified by RunSettingsId::MODEL_FORCE_SOLID_STRESS
/// boolean parameter. For fluid mode, the velocity divergence is computed from all particles,
/// while for solid mode, it only sums velocities of undamaged particles from the same body, or
/// fully damaged particles. This is needed to properly handle density derivatives in impact; it
/// is undesirable to get increased density by merely moving the bodies closer to each other.
///
/// Solver must use either this equation or some custom density computation, such as direct
/// summation (see \ref SummationSolver) or SPH formulation without solving the density (see \ref
/// DensityIndependentSolver).
class ContinuityEquation : public IEquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override;

    virtual void initialize(Storage& storage, ThreadPool& pool) override;

    virtual void finalize(Storage& storage, ThreadPool& pool) override;

    virtual void create(Storage& storage, IMaterial& material) const override;
};

/// \brief Helper term to keep smoothing length constant during the simulation
///
/// The smoothing length is currently stored as the 4th component of the position vectors. Because
/// of that, it is necessary to either provide an equation evolving smoothing length in time (see
/// \ref AdaptiveSmoothingLength), or use this equation to set all derivatives of smoothing length
/// to zero.
///
/// \attention If neither of these equations are used and the smoothing length is not explicitly
/// handled by the solver or timestepping, the behavior might by unexpected and possibly leads to
/// errors. This issue will be resolved in the future.
class ConstSmoothingLength : public IEquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override;

    virtual void initialize(Storage& storage, ThreadPool& pool) override;

    virtual void finalize(Storage& storage, ThreadPool& pool) override;

    virtual void create(Storage& storage, IMaterial& material) const override;
};


/// \brief Container holding equation terms.
///
/// Holds an array of equation terms. The object also defines operators for adding more equation
/// terms and functions \ref initialize, \ref finalize and \ref create, calling corresponding
/// functions for all stored equation terms. Equation terms are stored as shared pointers and can
/// be thus used in multiple EquationHolders. It is however not recommended to access the same
/// term concurrently as it can contain a state.
class EquationHolder {
private:
    Array<SharedPtr<IEquationTerm>> terms;

public:
    EquationHolder() = default;

    explicit EquationHolder(const SharedPtr<IEquationTerm>& term) {
        if (term != nullptr) {
            terms.push(term);
        }
    }

    EquationHolder& operator+=(const EquationHolder& other) {
        terms.pushAll(other.terms);
        return *this;
    }

    EquationHolder operator+(const EquationHolder& other) const {
        EquationHolder holder;
        holder += *this;
        holder += other;
        return holder;
    }

    /// Checks if the holder contains term of given type.
    template <typename Term>
    bool contains() const {
        static_assert(std::is_base_of<IEquationTerm, Term>::value, "Can be used only for terms");
        for (auto& ptr : terms) {
            IEquationTerm& term = *ptr;
            if (typeid(term) == typeid(Term)) {
                return true;
            }
        }
        return false;
    }

    /// Calls \ref EquationTerm::setDerivatives for all stored equation terms.
    void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) const {
        for (auto& t : terms) {
            t->setDerivatives(derivatives, settings);
        }
    }

    /// Calls \ref EquationTerm::initialize for all stored equation terms.
    void initialize(Storage& storage, ThreadPool& pool) {
        PROFILE_SCOPE("EquationHolder::initialize");
        for (auto& t : terms) {
            t->initialize(storage, pool);
        }
    }

    /// Calls \ref EquationTerm::finalize for all stored equation terms.
    void finalize(Storage& storage, ThreadPool& pool) {
        PROFILE_SCOPE("EquationHolder::finalize");
        for (auto& t : terms) {
            t->finalize(storage, pool);
        }
    }

    /// Calls \ref EquationTerm::create for all stored equation terms.
    void create(Storage& storage, IMaterial& material) const {
        for (auto& t : terms) {
            t->create(storage, material);
        }
    }

    Size getTermCnt() const {
        return terms.size();
    }
};

template <typename Term, typename... TArgs>
INLINE EquationHolder makeTerm(TArgs&&... args) {
    return EquationHolder(makeShared<Term>(std::forward<TArgs>(args)...));
}

NAMESPACE_SPH_END
