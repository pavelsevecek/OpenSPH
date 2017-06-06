#pragma once

/// \file EquationTerm.h
/// \brief Right-hand side term of SPH equations
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/wrappers/SharedPtr.h"
#include "physics/Constants.h"
#include "sph/Material.h"
#include "sph/equations/Derivative.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    /// \brief Represents a term or terms appearing in evolutionary equations.
    ///
    /// Each EquationTerm either directly modifies quantities, or adds quantity derivatives. These terms never
    /// work directly with pairs of particles, instead they can register Abstract::Derivative that will be
    /// accumulated by the solver. EquationTerm can then access the result.
    class EquationTerm : public Polymorphic {
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
        virtual void create(Storage& storage, Abstract::Material& material) const = 0;
    };
}

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

/// Computes acceleration from pressure gradient and corresponding derivative of internal energy.
class PressureForce : public Abstract::EquationTerm {
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

        storage.parallelFor(0, du.size(), [&](const Size n1, const Size n2) INL {
            for (Size i = n1; i < n2; ++i) {
                du[i] -= p[i] / rho[i] * divv[i];
            }
        });
    }

    virtual void create(Storage& storage, Abstract::Material& material) const override {
        if (!dynamic_cast<EosMaterial*>(&material)) {
            throw InvalidSetup("PressureForce needs to be used with EosMaterial or derived");
        }
        const Float u0 = material.getParam<Float>(BodySettingsId::ENERGY);
        storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, u0);
        ASSERT(storage.getMaterialCnt() == 1);
        material.minimal(QuantityId::ENERGY) = material.getParam<Float>(BodySettingsId::ENERGY_MIN);
        material.range(QuantityId::ENERGY) = material.getParam<Range>(BodySettingsId::ENERGY_RANGE);
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

/// Computes acceleration from stress divergence. The stress is evolved as a first-order quantity,
/// using Hooke's law as constitutive equation. This represents solid bodies, for fluids use \ref
/// NavierStokesForce.
/// \todo add PressureForce together with this, it doesn't make sense to use stress force and NOT
/// pressure.
class SolidStressForce : public Abstract::EquationTerm {
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
            storage.parallelFor(*seq.begin(), *seq.end(), [&](const Size n1, const Size n2) INL {
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
            storage.parallelFor(0, C.size(), [&C](const Size n1, const Size n2) INL {
                for (Size i = n1; i < n2; ++i) {
                    C[i] = C[i].inverse();
                }
            });
        }
    }

    virtual void create(Storage& storage, Abstract::Material& material) const override {
        storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS,
            OrderEnum::FIRST,
            material.getParam<TracelessTensor>(BodySettingsId::STRESS_TENSOR));
        material.minimal(QuantityId::DEVIATORIC_STRESS) =
            material.getParam<Float>(BodySettingsId::STRESS_TENSOR_MIN);

        storage.insert<SymmetricTensor>(
            QuantityId::STRENGTH_VELOCITY_GRADIENT, OrderEnum::ZERO, SymmetricTensor::null());
    }
};

class NavierStokesForce : public Abstract::EquationTerm {
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

    virtual void create(Storage& storage, Abstract::Material& material) const override {
        ASSERT(storage.has(QuantityId::ENERGY) && storage.has(QuantityId::PRESSURE));
        storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS,
            OrderEnum::ZERO,
            material.getParam<TracelessTensor>(BodySettingsId::STRESS_TENSOR));
    }
};

/// Equation for evolution of density. Solver must use either this equation or some custom density
/// computation, such as direct summation (see SummationSolver) or SPH formulation without solving the density
/// (see DensityIndependentSolver).
class ContinuityEquation : public Abstract::EquationTerm {
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
            storage.parallelFor(0, rho.size(), [&](const Size n1, const Size n2) INL {
                for (Size i = n1; i < n2; ++i) {
                    if (reduce[i] != 0._f) {
                        drho[i] = -rho[i] * gradv[i].trace();
                    } else {
                        drho[i] = -rho[i] * divv[i];
                    }
                }
            });
        } else {
            storage.parallelFor(0, rho.size(), [&](const Size n1, const Size n2) INL {
                for (Size i = n1; i < n2; ++i) {
                    drho[i] = -rho[i] * divv[i];
                }
            });
        }
    }

    virtual void create(Storage& storage, Abstract::Material& material) const override {
        const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
        storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, rho0);
        material.minimal(QuantityId::DENSITY) = material.getParam<Float>(BodySettingsId::DENSITY_MIN);
        material.range(QuantityId::DENSITY) = material.getParam<Range>(BodySettingsId::DENSITY_RANGE);
        storage.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
    }
};

class AdaptiveSmoothingLength : public Abstract::EquationTerm {
private:
    struct {
        Float strength;
        Range range;
    } enforcing;

    Size dimensions;
    Float minimal;

public:
    explicit AdaptiveSmoothingLength(const RunSettings& settings, const Size dimensions = DIMENSIONS)
        : dimensions(dimensions) {
        Flags<SmoothingLengthEnum> flags = Flags<SmoothingLengthEnum>::fromValue(
            settings.get<int>(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH));
        (void)flags;
        if (false) { // flags.has(SmoothingLengthEnum::SOUND_SPEED_ENFORCING)) {
            enforcing.strength = settings.get<Float>(RunSettingsId::SPH_NEIGHBOUR_ENFORCING);
            enforcing.range = settings.get<Range>(RunSettingsId::SPH_NEIGHBOUR_RANGE);
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
        storage.parallelFor(0, r.size(), [&](const Size n1, const Size n2) INL {
            for (Size i = n1; i < n2; ++i) {
                // 'continuity equation' for smoothing lengths
                v[i][H] = r[i][H] / dimensions * divv[i];

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

    virtual void create(Storage& UNUSED(storage), Abstract::Material& UNUSED(material)) const override {}
};


/// \brief Container holding equation terms
///
/// Holds an array of equation terms. The object also defines operators for adding more equation terms and
/// functions \ref initialize, \ref finalize and \ref create, calling corresponding functions for all stored
/// equation terms.
/// Equation terms are stored as shared pointers and can be thus used in multiple EquationHolders. It is
/// however not recommended to access the same term concurrently as it can contain a state.
class EquationHolder {
private:
    Array<SharedPtr<Abstract::EquationTerm>> terms;

public:
    EquationHolder() = default;

    EquationHolder(const SharedPtr<Abstract::EquationTerm>& term) {
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

    void setupThread(DerivativeHolder& derivatives, const RunSettings& settings) const {
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
    void create(Storage& storage, Abstract::Material& material) const {
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
