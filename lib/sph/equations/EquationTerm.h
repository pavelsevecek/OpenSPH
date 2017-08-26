#pragma once

/// \file EquationTerm.h
/// \brief Right-hand side term of SPH equations
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

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
    /// Sets derivatives required by this term. The derivatives are then automatically evaluated by the
    /// solver, the equation term can access the result in \ref finalize function. This function is called
    /// once for each thread at the beginning of the run.
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) = 0;

    /// Initialize all the derivatives and/or quantity values before derivatives are computed. Called at
    /// the beginning of every time step. Note that derivatives need not be zeroed out manually, this is
    /// already done by timestepping (for derivatives of quantities) and solver (for accumulated values).
    virtual void initialize(Storage& storage) = 0;

    /// Computes all the derivatives and/or quantity values based on accumulated derivatives. Called every
    /// time step after derivatives are evaluated and saved to storage.
    virtual void finalize(Storage& storage) = 0;

    /// Creates all quantities needed by the term using given material. Called once for every body in
    /// the simulation.
    virtual void create(Storage& storage, IMaterial& material) const = 0;

    /// \brief Fills given arrays with type_info of other EquationTerm objects, specifying compatibility
    /// with other objects.
    ///
    /// This is used as sanity check of the setup. By default, it is assumed that all equation term are
    /// compatible with each other and may be used together.
    /// \param demands Types of equation terms that MUST be solved together with this term
    /// \param forbids Types of equation terms that MUST NOT be solved together with tihs term
    /// \todo This should be somehow generalized; for example we should only include one arficifial
    /// viscosity without the need to specify EVERY single AV in 'forbids' list.
    virtual void requires(Array<std::type_info>& UNUSED(demands),
        Array<std::type_info>& UNUSED(forbids)) const {}
};


class PressureGradient : public DerivativeTemplate<PressureGradient> {
private:
    ArrayView<const Float> p, rho, m;
    ArrayView<Vector> dv;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(p, rho, m) =
            input.getValues<Float>(QuantityId::PRESSURE, QuantityId::DENSITY, QuantityId::MASSES);
        dv = results.getBuffer<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        ASSERT(neighs.size() == grads.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            const Vector f = (p[i] + p[j]) / (rho[i] * rho[j]) * grads[k];
            ASSERT(isReal(f));
            dv[i] -= m[j] * f;
            if (Symmetrize) {
                dv[j] += m[i] * f;
            }
        }
    }
};

/// \brief Equation of motion due to pressure gradient
///
/// Computes acceleration from pressure gradient and corresponding derivative of internal energy.
/// \todo
class PressureForce : public IEquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<PressureGradient>(settings);
        derivatives.require<VelocityDivergence>(settings);
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override {
        ArrayView<const Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
        ArrayView<const Float> p, rho, m; ///\todo share these arrayviews?
        tie(p, rho, m) =
            storage.getValues<Float>(QuantityId::PRESSURE, QuantityId::DENSITY, QuantityId::MASSES);
        ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);

        parallelFor(0, du.size(), [&](const Size n1, const Size n2) INL {
            for (Size i = n1; i < n2; ++i) {
                du[i] -= p[i] / rho[i] * divv[i];
            }
        });
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        if (!dynamic_cast<EosMaterial*>(&material)) {
            throw InvalidSetup("PressureForce needs to be used with EosMaterial or derived");
        }
        const Float u0 = material.getParam<Float>(BodySettingsId::ENERGY);
        storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, u0);
        ASSERT(storage.getMaterialCnt() == 1);
        material.setRange(QuantityId::ENERGY, BodySettingsId::ENERGY_RANGE, BodySettingsId::ENERGY_MIN);
        // need to create quantity for velocity divergence so that we can save it to storage later
        storage.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
    }
};

class StressDivergence : public DerivativeTemplate<StressDivergence> {
private:
    ArrayView<const Float> rho, m;
    ArrayView<const TracelessTensor> s;
    ArrayView<const Float> reduce;
    ArrayView<const Size> flag;
    ArrayView<Vector> dv;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASSES);
        s = input.getPhysicalValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
        reduce = input.getValue<Float>(QuantityId::STRESS_REDUCING);
        flag = input.getValue<Size>(QuantityId::FLAG);
        dv = results.getBuffer<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND);
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        ASSERT(neighs.size() == grads.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            if (flag[i] != flag[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
                continue;
            }
            const Vector f = (s[i] + s[j]) / (rho[i] * rho[j]) * grads[k];
            ASSERT(isReal(f));
            dv[i] += m[j] * f;
            if (Symmetrize) {
                dv[j] -= m[i] * f;
            }
        }
    }
};

/// \brief Equation of motion for solid body and constitutive equation for the stress tensor (Hooke's law)
///
/// The equation computes acceleration from the divergence of the deviatoric stress \f$S\f$. The equation of
/// motion in the used SPH discretization reads:
/// \f[
///  \frac{{\rm d} \vec v_i}{{\rm d} t} = \sum_j m_j \left(\frac{S_i + S_j}{\rho_i \rho_j}\right) \cdot
///  \nabla W_{ij}\,,
/// \f]
/// Corresponding term of energy equation (viscous heating) is also added to the energy derivative:
/// \f[
///  \frac{{\rm d} u_i}{{\rm d} t} = \frac{1}{\rho_i} S_i : \nabla \vec v_i \,,
/// \f]
/// where \f$\nabla \vec v_i\f$ is the symmetrized velocity gradient.
///
/// The stress is evolved as a first-order quantity, using Hooke's law as constitutive equation:
/// \f[
///  \frac{{\rm d} S_i}{{\rm d} t} = 2\mu \left( \nabla \vec v - {\bf 1} \, \frac{\nabla \cdot \vec v}{3}
///  \right)\,,
/// \f]
/// where \f$\mu\f$ is the shear modulus and \f$\bf 1\f$ is the identity tensor.
///
/// This equation represents solid bodies, for fluids use \ref NavierStokesForce.
///
/// \attention The isotropic part of the force is NOT computed, it is necessary to use \ref PressureForce
/// together with this equation.
class SolidStressForce : public IEquationTerm {
private:
    bool conserveAngularMomentum;

public:
    explicit SolidStressForce(const RunSettings& settings) {
        conserveAngularMomentum = settings.get<bool>(RunSettingsId::SPH_CONSERVE_ANGULAR_MOMENTUM);
    }

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<StressDivergence>(settings);
        using namespace VelocityGradientCorrection;
        if (conserveAngularMomentum) {
            derivatives.require<StrengthVelocityGradient<ConserveAngularMomentum>>(settings);
        } else {
            derivatives.require<StrengthVelocityGradient<NoCorrection>>(settings);
        }
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override {
        ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
        ArrayView<TracelessTensor> s, ds;
        tie(s, ds) = storage.getPhysicalAll<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
        ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);
        ArrayView<SymmetricTensor> gradv =
            storage.getValue<SymmetricTensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT);

        for (Size matIdx = 0; matIdx < storage.getMaterialCnt(); ++matIdx) {
            MaterialView material = storage.getMaterial(matIdx);
            const Float mu = material->getParam<Float>(BodySettingsId::SHEAR_MODULUS);
            IndexSequence seq = material.sequence();
            parallelFor(*seq.begin(), *seq.end(), [&](const Size n1, const Size n2) INL {
                for (Size i = n1; i < n2; ++i) {
                    du[i] += 1._f / rho[i] * ddot(s[i], gradv[i]);
                    /// \todo rotation rate tensor?
                    TracelessTensor dev(gradv[i] - SymmetricTensor::identity() * gradv[i].trace() / 3._f);
                    ds[i] += 2._f * mu * dev;
                    ASSERT(isReal(du[i]) && isReal(ds[i]));
                }
            });
        }
        if (conserveAngularMomentum) {
            /// \todo here we assume only this equation term uses the correction
            ArrayView<SymmetricTensor> C =
                storage.getValue<SymmetricTensor>(QuantityId::ANGULAR_MOMENTUM_CORRECTION);
            parallelFor(0, C.size(), [&C](const Size n1, const Size n2) INL {
                for (Size i = n1; i < n2; ++i) {
                    C[i] = C[i].inverse();
                }
            });
        }
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS,
            OrderEnum::FIRST,
            material.getParam<TracelessTensor>(BodySettingsId::STRESS_TENSOR));
        material.setRange(QuantityId::DEVIATORIC_STRESS,
            Interval::unbounded(),
            material.getParam<Float>(BodySettingsId::STRESS_TENSOR_MIN));

        storage.insert<SymmetricTensor>(
            QuantityId::STRENGTH_VELOCITY_GRADIENT, OrderEnum::ZERO, SymmetricTensor::null());
    }
};

/// \brief Navier-Stokes equation of motion
///
/// \todo
class NavierStokesForce : public IEquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<StressDivergence>(settings);
        // do don't need to do 'hacks' with gradient for fluids
        derivatives.require<VelocityGradient>(settings);
    }

    virtual void initialize(Storage&) override {
        TODO("implement");
    }

    virtual void finalize(Storage& storage) override {
        ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
        ArrayView<TracelessTensor> s, ds;
        tie(s, ds) = storage.getPhysicalAll<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
        ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);
        ArrayView<SymmetricTensor> gradv =
            storage.getValue<SymmetricTensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT);

        TODO("parallelize");
        for (Size matIdx = 0; matIdx < storage.getMaterialCnt(); ++matIdx) {
            MaterialView material = storage.getMaterial(matIdx);
            const Float mu = material->getParam<Float>(BodySettingsId::SHEAR_MODULUS);
            for (Size i : material.sequence()) {
                du[i] += 1._f / rho[i] * ddot(s[i], gradv[i]);
                /// \todo rotation rate tensor?
                TracelessTensor dev(gradv[i] - SymmetricTensor::identity() * gradv[i].trace() / 3._f);
                ds[i] += 2._f * mu * dev;
                ASSERT(isReal(du[i]) && isReal(ds[i]));
            }
        }
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        ASSERT(storage.has(QuantityId::ENERGY) && storage.has(QuantityId::PRESSURE));
        storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS,
            OrderEnum::ZERO,
            material.getParam<TracelessTensor>(BodySettingsId::STRESS_TENSOR));
    }
};

/// \brief Equation for evolution of density
///
/// Provides a first-order evolutionary equation for density, computing the derivative of i-th particle using:
/// \f[
///  \frac{{\rm d} \rho_i}{{\rm d} t} = -\rho_i \nabla \cdot \vec v_i \,,
/// \f]
/// where \f$\vec v_i\f$ is the velocity of the particle.
///
/// The equation has two modes - fluid and solid - differing in the particle set used in the sum of velocity
/// divergence. The modes are specified by RunSettingsId::MODEL_FORCE_SOLID_STRESS boolean parameter.
/// For fluid mode, the velocity divergence is computed from all particles, while for solid mode, it only sums
/// velocities of undamaged particles from the same body, or fully damaged particles. This is needed to
/// properly handle density derivatives in impact; it is undesirable to get increased density by merely moving
/// the bodies closer to each other.
///
/// Solver must use either this equation or some custom density computation, such as direct summation (see
/// \ref SummationSolver) or SPH formulation without solving the density (see \ref DensityIndependentSolver).
class ContinuityEquation : public IEquationTerm {
private:
    enum class Options {
        /// All particles contribute to the density derivative (default option)
        FLUID = 0,

        /// Only particles from the same body OR fully damaged particles contribute
        /// \todo possibly allow per-material settings here
        SOLID = 1 << 0,
    };

    Flags<Options> flags;

public:
    explicit ContinuityEquation(const RunSettings& settings) {
        flags.setIf(Options::SOLID, settings.get<bool>(RunSettingsId::MODEL_FORCE_SOLID_STRESS));
    }

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<VelocityDivergence>(settings);
        if (flags.has(Options::SOLID)) {
            using namespace VelocityGradientCorrection;
            derivatives.require<StrengthVelocityGradient<NoCorrection>>(settings);
        }
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override {
        ArrayView<const Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
        ArrayView<Float> rho, drho;
        tie(rho, drho) = storage.getAll<Float>(QuantityId::DENSITY);
        if (flags.has(Options::SOLID)) {
            ArrayView<const Float> reduce = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
            ArrayView<const SymmetricTensor> gradv =
                storage.getValue<SymmetricTensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT);
            parallelFor(0, rho.size(), [&](const Size n1, const Size n2) INL {
                for (Size i = n1; i < n2; ++i) {
                    if (reduce[i] != 0._f) {
                        drho[i] = -rho[i] * gradv[i].trace();
                    } else {
                        drho[i] = -rho[i] * divv[i];
                    }
                }
            });
        } else {
            parallelFor(0, rho.size(), [&](const Size n1, const Size n2) INL {
                for (Size i = n1; i < n2; ++i) {
                    drho[i] = -rho[i] * divv[i];
                }
            });
        }
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
        storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, rho0);
        material.setRange(QuantityId::DENSITY, BodySettingsId::DENSITY_RANGE, BodySettingsId::DENSITY_MIN);
        storage.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
    }
};

/// \brief Evolutionary equation for the (scalar) smoothing length
///
/// The smoothing length is evolved using first-order equation, 'mirroring' changes in density in order to
/// keep the particle concentration approximately constant. The derivative of smoothing length \f$h\f$ of i-th
/// particle computed using:
/// \f[
///  \frac{{\rm d} h_i}{{\rm d} t} = \frac{h_i}{D} \nabla \cdot \vec v_i \,,
/// \f]
/// where \f$D\f$ is the number of spatial dimensions (3 unless specified otherwise) and \f$\vec v_i\f$ is the
/// velocity of the particle.
///
/// It is possible to add additional term into the equation that increases/decreases the smoothing length when
/// number of neighbours is too low/high. This is done by setting flag
/// SmoothingLengthEnum::SOUND_SPEED_ENFORCING to settings entry RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH. The
/// allowed range of neighbours is then controlled by RunSettingsId::SPH_NEIGHBOUR_RANGE. Note that the number
/// of neighbours is not guaranteed to be in the given range, the actual number of neighbours cannot be
/// precisely controlled.
class AdaptiveSmoothingLength : public IEquationTerm {
private:
    struct {
        Float strength;
        Interval range;
    } enforcing;

    Size dimensions;
    Float minimal;

public:
    explicit AdaptiveSmoothingLength(const RunSettings& settings, const Size dimensions = DIMENSIONS)
        : dimensions(dimensions) {
        Flags<SmoothingLengthEnum> flags = Flags<SmoothingLengthEnum>::fromValue(
            settings.get<int>(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH));
        MARK_USED(flags);
        if (false) { // flags.has(SmoothingLengthEnum::SOUND_SPEED_ENFORCING)) {
            enforcing.strength = settings.get<Float>(RunSettingsId::SPH_NEIGHBOUR_ENFORCING);
            enforcing.range = settings.get<Interval>(RunSettingsId::SPH_NEIGHBOUR_RANGE);
        } else {
            enforcing.strength = -INFTY;
        }
        minimal = settings.get<Float>(RunSettingsId::SPH_SMOOTHING_LENGTH_MIN);
    }

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<VelocityDivergence>(settings);
    }

    virtual void initialize(Storage& storage) override {
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
        // clamp smoothing lengths
        for (Size i = 0; i < r.size(); ++i) {
            r[i][H] = max(r[i][H], minimal);
        }
    }

    virtual void finalize(Storage& storage) override {
        ArrayView<const Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
        ArrayView<const Float> cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
        ArrayView<const Size> neighCnt = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
        parallelFor(0, r.size(), [&](const Size n1, const Size n2) INL {
            for (Size i = n1; i < n2; ++i) {
                // 'continuity equation' for smoothing lengths
                if (r[i][H] > 2._f * minimal) {
                    v[i][H] = r[i][H] / dimensions * divv[i];
                } else {
                    v[i][H] = 0._f;
                }

                /// \todo generalize for grad v
                // no acceleration of smoothing lengths (we evolve h as first-order quantity)
                dv[i][H] = 0._f;

                if (enforcing.strength > -1.e2_f) {
                    // check upper limit of neighbour count
                    const Float dn1 = neighCnt[i] - enforcing.range.upper();
                    ASSERT(dn1 < r.size());
                    if (dn1 > 0._f) {
                        // sound speed is used to add correct dimensions to the term
                        v[i][H] -= exp(enforcing.strength * dn1) * cs[i];
                        continue;
                    }
                    // check lower limit of neighbour count
                    const Float dn2 = enforcing.range.lower() - neighCnt[i];
                    ASSERT(dn2 < r.size());
                    if (dn2 > 0._f) {
                        v[i][H] += exp(enforcing.strength * dn2) * cs[i];
                    }
                }
                ASSERT(isReal(v[i]));
            }
        });
    }

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};

/// \brief Helper term to keep smoothing length constant during the simulation
///
/// The smoothing length is currently stored as the 4th component of the position vectors. Because of that, it
/// is necessary to either provide an equation evolving smoothing length in time (see \ref
/// AdaptiveSmoothingLength), or use this equation to set all derivatives of smoothing length to zero.
///
/// \attention If neither of these equations are used and the smoothing length is not explicitly handled by
/// the solver or timestepping, the behavior might by unexpected and possibly leads to errors. This issue will
/// be resolved in the future.
class ConstSmoothingLength : public IEquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& UNUSED(derivatives),
        const RunSettings& UNUSED(settings)) override {}

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
        parallelFor(0, r.size(), [&v, &dv](const Size n1, const Size n2) INL {
            for (Size i = n1; i < n2; ++i) {
                v[i][H] = 0._f;
                dv[i][H] = 0._f;
            }
        });
    }

    virtual void create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const override {}
};


/// \brief Container holding equation terms
///
/// Holds an array of equation terms. The object also defines operators for adding more equation terms and
/// functions \ref initialize, \ref finalize and \ref create, calling corresponding functions for all stored
/// equation terms. Equation terms are stored as shared pointers and can be thus used in multiple
/// EquationHolders. It is however not recommended to access the same term concurrently as it can contain a
/// state.
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
    void initialize(Storage& storage) {
        PROFILE_SCOPE("EquationHolder::initialize");
        for (auto& t : terms) {
            t->initialize(storage);
        }
    }

    /// Calls \ref EquationTerm::finalize for all stored equation terms.
    void finalize(Storage& storage) {
        PROFILE_SCOPE("EquationHolder::finalize");
        for (auto& t : terms) {
            t->finalize(storage);
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
