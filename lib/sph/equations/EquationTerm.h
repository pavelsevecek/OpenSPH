#pragma once

/// \file EquationTerm.h
/// \brief Right-hand side term of SPH equations
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

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

class PressureGradient : public Abstract::Derivative {
private:
    ArrayView<const Float> p, rho, m;
    ArrayView<Vector> dv;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Vector>(QuantityId::POSITIONS);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(p, rho, m) =
            input.getValues<Float>(QuantityId::PRESSURE, QuantityId::DENSITY, QuantityId::MASSES);
        dv = results.getValue<Vector>(QuantityId::POSITIONS);
    }

    virtual void compute(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) override {
        ASSERT(neighs.size() == grads.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            const Vector f = (p[i] + p[j]) / (rho[i] * rho[j]) * grads[k];
            ASSERT(isReal(f));
            dv[i] -= m[j] * f;
            dv[j] += m[i] * f;
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

        for (Size i = 0; i < du.size(); ++i) {
            du[i] -= p[i] / rho[i] * divv[i];
        }
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

class StressDivergence : public Abstract::Derivative {
private:
    ArrayView<const Float> rho, m;
    ArrayView<const TracelessTensor> s;
    ArrayView<Vector> dv;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Vector>(QuantityId::POSITIONS);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASSES);
        s = input.getPhysicalValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
        dv = results.getValue<Vector>(QuantityId::POSITIONS);
    }

    virtual void compute(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) override {
        ASSERT(neighs.size() == grads.size());
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            const Vector f = (s[i] + s[j]) / (rho[i] * rho[j]) * grads[k];
            ASSERT(isReal(f));
            dv[i] += m[j] * f;
            dv[j] -= m[i] * f;
        }
    }
};

/// Computes acceleration from stress divergence. The stress is evolved as a first-order quantity,
/// using Hooke's law as constitutive equation. This represents solid bodies, for fluids use \ref
/// NavierStokesForce.
/// \todo add PressureForce together with this, it doesn't make sense to use stress force and NOT pressure.
class SolidStressForce : public Abstract::EquationTerm {
private:
    bool conserveAngularMomentum;

public:
    SolidStressForce(const RunSettings& settings) {
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
        ArrayView<Tensor> gradv = storage.getValue<Tensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT);

        for (Size matIdx = 0; matIdx < storage.getMaterialCnt(); ++matIdx) {
            MaterialView material = storage.getMaterial(matIdx);
            const Float mu = material->getParam<Float>(BodySettingsId::SHEAR_MODULUS);
            for (Size i : material.sequence()) {
                du[i] += 1._f / rho[i] * ddot(s[i], gradv[i]);
                /// \todo rotation rate tensor?
                TracelessTensor dev(gradv[i] - Tensor::identity() * gradv[i].trace() / 3._f);
                ds[i] += 2._f * mu * dev;
                ASSERT(isReal(du[i]) && isReal(ds[i]));
            }
        }
        if (conserveAngularMomentum) {
            /// \todo here we assume only this equation term uses the correction
            ArrayView<Tensor> C = storage.getValue<Tensor>(QuantityId::ANGULAR_MOMENTUM_CORRECTION);
            for (Size i = 0; i < C.size(); ++i) {
                C[i] = C[i].inverse();
            }
        }
    }

    virtual void create(Storage& storage, Abstract::Material& material) const override {
        storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS,
            OrderEnum::FIRST,
            material.getParam<TracelessTensor>(BodySettingsId::STRESS_TENSOR));
        material.minimal(QuantityId::DEVIATORIC_STRESS) =
            material.getParam<Float>(BodySettingsId::STRESS_TENSOR_MIN);

        storage.insert<Tensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT, OrderEnum::ZERO, Tensor::null());
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
        ArrayView<Tensor> gradv = storage.getValue<Tensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT);

        for (Size matIdx = 0; matIdx < storage.getMaterialCnt(); ++matIdx) {
            MaterialView material = storage.getMaterial(matIdx);
            const Float mu = material->getParam<Float>(BodySettingsId::SHEAR_MODULUS);
            for (Size i : material.sequence()) {
                du[i] += 1._f / rho[i] * ddot(s[i], gradv[i]);
                /// \todo rotation rate tensor?
                TracelessTensor dev(gradv[i] - Tensor::identity() * gradv[i].trace() / 3._f);
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

class CentripetalForce : public Abstract::EquationTerm {
private:
    Float omega;

public:
    CentripetalForce(const RunSettings& settings) {
        omega = settings.get<Float>(RunSettingsId::FRAME_ANGULAR_FREQUENCY);
    }

    virtual void setDerivatives(DerivativeHolder& UNUSED(derivatives),
        const RunSettings& UNUSED(settings)) override {}

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override {
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
        const Vector unitZ = Vector(0._f, 0._f, 1._f);
        for (Size i = 0; i < r.size(); ++i) {
            dv[i] += omega * (r[i] - unitZ * dot(r[i], unitZ));
            // no energy term - energy is not generally conserved when external force is used
        }
    }

    virtual void create(Storage& UNUSED(storage), Abstract::Material& UNUSED(material)) const override {}
};

class ContinuityEquation : public Abstract::EquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<VelocityDivergence>(settings);
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& storage) override {
        ArrayView<const Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
        ArrayView<Float> rho, drho;
        tie(rho, drho) = storage.getAll<Float>(QuantityId::DENSITY);
        for (Size i = 0; i < rho.size(); ++i) {
            drho[i] = -rho[i] * divv[i];
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
    AdaptiveSmoothingLength(const RunSettings& settings, const Size dimensions = DIMENSIONS)
        : dimensions(dimensions) {
        Flags<SmoothingLengthEnum> flags = Flags<SmoothingLengthEnum>::fromValue(
            settings.get<int>(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH));
        if (flags.has(SmoothingLengthEnum::SOUND_SPEED_ENFORCING)) {
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
        for (Size i = 0; i < r.size(); ++i) {
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
    }

    virtual void create(Storage& UNUSED(storage), Abstract::Material& UNUSED(material)) const override {}
};

class NeighbourCountTerm : public Abstract::EquationTerm {
private:
    class NeighbourCountImpl : public Abstract::Derivative {
    private:
        ArrayView<Size> neighCnts;

    public:
        virtual void create(Accumulated& results) override {
            results.insert<Size>(QuantityId::NEIGHBOUR_CNT);
        }

        virtual void initialize(const Storage& UNUSED(input), Accumulated& results) override {
            neighCnts = results.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
        }

        virtual void compute(const Size i,
            ArrayView<const Size> neighs,
            ArrayView<const Vector> UNUSED_IN_RELEASE(grads)) override {
            ASSERT(neighs.size() == grads.size());
            neighCnts[i] += neighs.size();
            for (Size k = 0; k < neighs.size(); ++k) {
                const Size j = neighs[k];
                neighCnts[j]++;
            }
        }
    };

    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        derivatives.require<NeighbourCountImpl>(settings);
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& UNUSED(storage), Abstract::Material& UNUSED(material)) const override {}
};

/// Syntactic suggar
class EquationHolder {
private:
    Array<std::unique_ptr<Abstract::EquationTerm>> terms;

public:
    EquationHolder() = default;

    EquationHolder(std::unique_ptr<Abstract::EquationTerm>&& term) {
        terms.push(std::move(term));
    }

    EquationHolder& operator+=(EquationHolder&& other) {
        terms.pushAll(std::move(other.terms));
        return *this;
    }

    EquationHolder operator+(EquationHolder&& other) {
        EquationHolder holder;
        holder += std::move(other);
        return holder;
    }

    void setupThread(DerivativeHolder& derivatives, const RunSettings& settings) const {
        for (auto& t : terms) {
            t->setDerivatives(derivatives, settings);
        }
    }

    void initialize(Storage& storage) {
        PROFILE_SCOPE("EquationHolder::initialize");
        for (auto& t : terms) {
            t->initialize(storage);
        }
    }

    void finalize(Storage& storage) {
        PROFILE_SCOPE("EquationHolder::finalize");
        for (auto& t : terms) {
            t->finalize(storage);
        }
    }

    void create(Storage& storage, Abstract::Material& material) const {
        for (auto& t : terms) {
            t->create(storage, material);
        }
    }
};

template <typename Term, typename... TArgs>
INLINE EquationHolder makeTerm(TArgs&&... args) {
    return EquationHolder(std::make_unique<Term>(std::forward<TArgs>(args)...));
}

NAMESPACE_SPH_END
