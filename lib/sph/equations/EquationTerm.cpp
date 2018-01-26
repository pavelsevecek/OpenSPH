#include "sph/equations/EquationTerm.h"

NAMESPACE_SPH_BEGIN

template <typename TVariant>
class PressureGradient : public DerivativeTemplate<PressureGradient<TVariant>> {
private:
    ArrayView<const Float> p, rho, m;
    ArrayView<Vector> dv;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(p, rho, m) = input.getValues<Float>(QuantityId::PRESSURE, QuantityId::DENSITY, QuantityId::MASS);
        dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        const Vector f = TVariant::term(p, rho, i, j) * grad;
        ASSERT(isReal(f));
        dv[i] -= m[j] * f;
        if (Symmetrize) {
            dv[j] += m[i] * f;
        }
    }
};

void StandardSph::PressureForce::setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) {
    derivatives.require<DensityVelocityDivergence>(settings);

    struct Term {
        static Float term(ArrayView<const Float> p, ArrayView<const Float> rho, const Size i, const Size j) {
            return p[i] / sqr(rho[i]) + p[j] / sqr(rho[j]);
        }
    };
    derivatives.require<PressureGradient<Term>>(settings);
}

void StandardSph::PressureForce::initialize(Storage& UNUSED(storage)) {}

void StandardSph::PressureForce::finalize(Storage& storage) {
    ArrayView<const Float> p, rho;
    tie(p, rho) = storage.getValues<Float>(QuantityId::PRESSURE, QuantityId::DENSITY);
    ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);
    ArrayView<const Float> rhoDivv = storage.getValue<Float>(QuantityId::DENSITY_VELOCITY_DIVERGENCE);
    parallelFor(0, du.size(), [&](const Size n1, const Size n2) INL {
        for (Size i = n1; i < n2; ++i) {
            du[i] -= p[i] / sqr(rho[i]) * rhoDivv[i];
        }
    });
}

void StandardSph::PressureForce::create(Storage& storage, IMaterial& material) const {
    if (!dynamic_cast<EosMaterial*>(&material)) {
        throw InvalidSetup("PressureForce needs to be used with EosMaterial or derived");
    }
    const Float u0 = material.getParam<Float>(BodySettingsId::ENERGY);
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, u0);
    material.setRange(QuantityId::ENERGY, BodySettingsId::ENERGY_RANGE, BodySettingsId::ENERGY_MIN);
    // need to create quantity for density velocity divergence so that we can save it to storage later
    storage.insert<Float>(QuantityId::DENSITY_VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
}


void BenzAsphaugSph::PressureForce::setDerivatives(DerivativeHolder& derivatives,
    const RunSettings& settings) {
    derivatives.require<VelocityDivergence>(settings);

    struct Term {
        static Float term(ArrayView<const Float> p, ArrayView<const Float> rho, const Size i, const Size j) {
            return (p[i] + p[j]) / (rho[i] * rho[j]);
        }
    };
    derivatives.require<PressureGradient<Term>>(settings);
}

void BenzAsphaugSph::PressureForce::initialize(Storage& UNUSED(storage)) {}

void BenzAsphaugSph::PressureForce::finalize(Storage& storage) {
    ArrayView<const Float> p, rho;
    tie(p, rho) = storage.getValues<Float>(QuantityId::PRESSURE, QuantityId::DENSITY);
    ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);
    ArrayView<const Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
    parallelFor(0, du.size(), [&](const Size n1, const Size n2) INL {
        for (Size i = n1; i < n2; ++i) {
            du[i] -= p[i] / rho[i] * divv[i];
        }
    });
}

void BenzAsphaugSph::PressureForce::create(Storage& storage, IMaterial& material) const {
    if (!dynamic_cast<EosMaterial*>(&material)) {
        throw InvalidSetup("PressureForce needs to be used with EosMaterial or derived");
    }
    const Float u0 = material.getParam<Float>(BodySettingsId::ENERGY);
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, u0);
    material.setRange(QuantityId::ENERGY, BodySettingsId::ENERGY_RANGE, BodySettingsId::ENERGY_MIN);
    storage.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
}


template <typename Term>
class StressDivergence : public DerivativeTemplate<StressDivergence<Term>> {
private:
    ArrayView<const Float> rho, m;
    ArrayView<const TracelessTensor> s;
    ArrayView<const Float> reduce;
    ArrayView<const Size> flag;
    ArrayView<Vector> dv;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(rho, m) = input.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS);
        s = input.getPhysicalValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
        reduce = input.getValue<Float>(QuantityId::STRESS_REDUCING);
        flag = input.getValue<Size>(QuantityId::FLAG);
        dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        if (flag[i] != flag[j] || reduce[i] == 0._f || reduce[j] == 0._f) {
            return;
        }
        const Vector f = Term::term(s, rho, i, j) * grad;
        ASSERT(isReal(f));
        dv[i] += m[j] * f;
        if (Symmetrize) {
            dv[j] -= m[i] * f;
        }
    }
};

static void createSolidStressForce(Storage& storage,
    IMaterial& material,
    QuantityId GradId,
    bool useCorrectionTensor) {
    const TracelessTensor s0 = material.getParam<TracelessTensor>(BodySettingsId::STRESS_TENSOR);
    storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::FIRST, s0);
    const Float s_min = material.getParam<Float>(BodySettingsId::STRESS_TENSOR_MIN);
    material.setRange(QuantityId::DEVIATORIC_STRESS, Interval::unbounded(), s_min);

    storage.insert<SymmetricTensor>(GradId, OrderEnum::ZERO, SymmetricTensor::null());
    if (useCorrectionTensor) {
        storage.insert<SymmetricTensor>(
            QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO, SymmetricTensor::identity());
    }
}

StandardSph::SolidStressForce::SolidStressForce(const RunSettings& settings) {
    // the correction tensor is associated with strength (density) velocity gradient, which we are
    // creating in this term, so we need to also create the correction tensor (if requested)
    useCorrectionTensor = settings.get<bool>(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR);
}

void StandardSph::SolidStressForce::setDerivatives(DerivativeHolder& derivatives,
    const RunSettings& settings) {
    derivatives.require<StrengthDensityVelocityGradient>(settings);

    struct Term {
        static TracelessTensor term(ArrayView<const TracelessTensor> s,
            ArrayView<const Float> rho,
            const Size i,
            const Size j) {
            return SolidStressForce::forceTerm(s, rho, i, j);
        }
    };
    derivatives.require<StressDivergence<Term>>(settings);
}

void StandardSph::SolidStressForce::initialize(Storage& UNUSED(storage)) {}

void StandardSph::SolidStressForce::finalize(Storage& storage) {
    ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    ArrayView<TracelessTensor> s, ds;
    tie(s, ds) = storage.getPhysicalAll<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);
    ArrayView<SymmetricTensor> rhoGradv =
        storage.getValue<SymmetricTensor>(QuantityId::STRENGTH_DENSITY_VELOCITY_GRADIENT);

    for (Size matIdx = 0; matIdx < storage.getMaterialCnt(); ++matIdx) {
        MaterialView material = storage.getMaterial(matIdx);
        const Float mu = material->getParam<Float>(BodySettingsId::SHEAR_MODULUS);
        IndexSequence seq = material.sequence();
        parallelFor(*seq.begin(), *seq.end(), [&](const Size n1, const Size n2) INL {
            for (Size i = n1; i < n2; ++i) {
                du[i] += 1._f / sqr(rho[i]) * ddot(s[i], rhoGradv[i]);
                /// \todo rotation rate tensor?
                TracelessTensor dev(rhoGradv[i] - SymmetricTensor::identity() * rhoGradv[i].trace() / 3._f);
                ds[i] += 2._f * mu / rho[i] * dev;
                ASSERT(isReal(du[i]) && isReal(ds[i]));
            }
        });
    }
}

void StandardSph::SolidStressForce::create(Storage& storage, IMaterial& material) const {
    createSolidStressForce(
        storage, material, QuantityId::STRENGTH_DENSITY_VELOCITY_GRADIENT, useCorrectionTensor);
}

BenzAsphaugSph::SolidStressForce::SolidStressForce(const RunSettings& settings) {
    useCorrectionTensor = settings.get<bool>(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR);
}

void BenzAsphaugSph::SolidStressForce::setDerivatives(DerivativeHolder& derivatives,
    const RunSettings& settings) {
    derivatives.require<StrengthVelocityGradient>(settings);

    struct Term {
        INLINE static TracelessTensor term(ArrayView<const TracelessTensor> s,
            ArrayView<const Float> rho,
            const Size i,
            const Size j) {
            return SolidStressForce::forceTerm(s, rho, i, j);
        }
    };
    derivatives.require<StressDivergence<Term>>(settings);
}

void BenzAsphaugSph::SolidStressForce::initialize(Storage& UNUSED(storage)) {}

void BenzAsphaugSph::SolidStressForce::finalize(Storage& storage) {
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
}

void BenzAsphaugSph::SolidStressForce::create(Storage& storage, IMaterial& material) const {
    createSolidStressForce(storage, material, QuantityId::STRENGTH_VELOCITY_GRADIENT, useCorrectionTensor);
}

void StandardSph::NavierStokesForce::setDerivatives(DerivativeHolder& derivatives,
    const RunSettings& settings) {
    /// derivatives.require<StressDivergence>(derivatives, settings);
    TODO("implement");
    // no need to do 'hacks' with gradient for fluids
    derivatives.require<VelocityGradient>(settings);
}

void StandardSph::NavierStokesForce::initialize(Storage&) {
    TODO("implement");
}

void StandardSph::NavierStokesForce::finalize(Storage& storage) {
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

void StandardSph::NavierStokesForce::create(Storage& storage, IMaterial& material) const {
    ASSERT(storage.has(QuantityId::ENERGY) && storage.has(QuantityId::PRESSURE));
    storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS,
        OrderEnum::ZERO,
        material.getParam<TracelessTensor>(BodySettingsId::STRESS_TENSOR));
}


void StandardSph::ContinuityEquation::setDerivatives(DerivativeHolder& derivatives,
    const RunSettings& settings) {
    derivatives.require<DensityVelocityDivergence>(settings);

    if (settings.get<bool>(RunSettingsId::MODEL_FORCE_SOLID_STRESS)) {
        /// \todo better way to "detect" if we need this gradient?
        derivatives.require<StrengthDensityVelocityGradient>(settings);
    }
}

void StandardSph::ContinuityEquation::initialize(Storage& UNUSED(storage)) {}

void StandardSph::ContinuityEquation::finalize(Storage& storage) {
    ArrayView<Float> rho, drho;
    tie(rho, drho) = storage.getAll<Float>(QuantityId::DENSITY);
    ArrayView<const Float> rhoDivv = storage.getValue<Float>(QuantityId::DENSITY_VELOCITY_DIVERGENCE);

    // see Benz&Asphaug version for commentary
    if (storage.has(QuantityId::STRENGTH_DENSITY_VELOCITY_GRADIENT)) {
        ArrayView<const SymmetricTensor> rhoGradv =
            storage.getValue<SymmetricTensor>(QuantityId::STRENGTH_DENSITY_VELOCITY_GRADIENT);
        ArrayView<const Float> reduce = storage.getValue<Float>(QuantityId::STRESS_REDUCING);

        parallelFor(0, rho.size(), [&](const Size n1, const Size n2) INL {
            for (Size i = n1; i < n2; ++i) {
                if (reduce[i] != 0._f) {
                    drho[i] = -rhoGradv[i].trace();
                } else {
                    drho[i] = -rhoDivv[i];
                }
            }
        });
    } else {
        parallelFor(0, rho.size(), [&](const Size n1, const Size n2) INL {
            for (Size i = n1; i < n2; ++i) {
                drho[i] = -rhoDivv[i];
            }
        });
    }
}

void StandardSph::ContinuityEquation::create(Storage& storage, IMaterial& material) const {
    const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, rho0);
    material.setRange(QuantityId::DENSITY, BodySettingsId::DENSITY_RANGE, BodySettingsId::DENSITY_MIN);

    storage.insert<Float>(QuantityId::DENSITY_VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
}

void BenzAsphaugSph::ContinuityEquation::setDerivatives(DerivativeHolder& derivatives,
    const RunSettings& settings) {
    // this formulation uses equation \dot \rho_i = m_i \sum_j m_j/rho_j \nabla \cdot \vec  v where the
    // velocity divergence is taken either directly, or as a trace of strength velocity gradient, see
    // below.
    derivatives.require<VelocityDivergence>(settings);

    if (settings.get<bool>(RunSettingsId::MODEL_FORCE_SOLID_STRESS)) {
        /// \todo better way to "detect" if we need this gradient?
        derivatives.require<StrengthVelocityGradient>(settings);
    }
}

void BenzAsphaugSph::ContinuityEquation::initialize(Storage& UNUSED(storage)) {}

void BenzAsphaugSph::ContinuityEquation::finalize(Storage& storage) {
    ArrayView<Float> rho, drho;
    tie(rho, drho) = storage.getAll<Float>(QuantityId::DENSITY);
    ArrayView<const Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);

    if (storage.has(QuantityId::STRENGTH_VELOCITY_GRADIENT)) {
        // for solid materials, we use the trace of the strength velocity gradient instead of velocity
        // divergence for particles with damage>0, necessary to avoid increasing density when two bodies
        // approach (only the deformation on impact increases the density)
        ArrayView<const SymmetricTensor> gradv =
            storage.getValue<SymmetricTensor>(QuantityId::STRENGTH_VELOCITY_GRADIENT);
        ArrayView<const Float> reduce = storage.getValue<Float>(QuantityId::STRESS_REDUCING);

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
        ASSERT(!storage.has(QuantityId::STRESS_REDUCING)); // sanity check

        // run without stress tensor, simply use the velocity divergence
        parallelFor(0, rho.size(), [&](const Size n1, const Size n2) INL {
            for (Size i = n1; i < n2; ++i) {
                drho[i] = -rho[i] * divv[i];
            }
        });
    }
}

void BenzAsphaugSph::ContinuityEquation::create(Storage& storage, IMaterial& material) const {
    const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, rho0);
    material.setRange(QuantityId::DENSITY, BodySettingsId::DENSITY_RANGE, BodySettingsId::DENSITY_MIN);
    storage.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
}

AdaptiveSmoothingLengthBase::AdaptiveSmoothingLengthBase(const RunSettings& settings, const Size dimensions)
    : dimensions(dimensions) {
    Flags<SmoothingLengthEnum> flags =
        settings.getFlags<SmoothingLengthEnum>(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH);
    if (flags.has(SmoothingLengthEnum::SOUND_SPEED_ENFORCING)) {
        enforcing.strength = settings.get<Float>(RunSettingsId::SPH_NEIGHBOUR_ENFORCING);
        enforcing.range = settings.get<Interval>(RunSettingsId::SPH_NEIGHBOUR_RANGE);
    } else {
        enforcing.strength = -INFTY;
    }
    minimal = settings.get<Float>(RunSettingsId::SPH_SMOOTHING_LENGTH_MIN);
}

void AdaptiveSmoothingLengthBase::initialize(Storage& storage) {
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    // clamp smoothing lengths
    for (Size i = 0; i < r.size(); ++i) {
        r[i][H] = max(r[i][H], minimal);
    }
}

INLINE void AdaptiveSmoothingLengthBase::enforce(const Size i,
    ArrayView<Vector> v,
    ArrayView<const Float> cs,
    ArrayView<const Size> neighCnt) {
    if (enforcing.strength <= -1.e2_f) {
        // too weak enforcing, effectively has no effect
        return;
    }

    // check upper limit of neighbour count
    const Float dn1 = neighCnt[i] - enforcing.range.upper();
    ASSERT(dn1 < neighCnt.size());
    if (dn1 > 0._f) {
        // sound speed is used to add correct dimensions to the term
        v[i][H] -= exp(enforcing.strength * dn1) * cs[i];
        return;
    }
    // check lower limit of neighbour count
    const Float dn2 = enforcing.range.lower() - neighCnt[i];
    ASSERT(dn2 < neighCnt.size());
    if (dn2 > 0._f) {
        v[i][H] += exp(enforcing.strength * dn2) * cs[i];
    }

    ASSERT(isReal(v[i]));
}

void StandardSph::AdaptiveSmoothingLength::setDerivatives(DerivativeHolder& derivatives,
    const RunSettings& settings) {
    derivatives.require<DensityVelocityDivergence>(settings);
}

void StandardSph::AdaptiveSmoothingLength::finalize(Storage& storage) {
    ArrayView<const Float> rhoDivv, cs, rho;
    tie(rhoDivv, cs, rho) = storage.getValues<Float>(
        QuantityId::DENSITY_VELOCITY_DIVERGENCE, QuantityId::SOUND_SPEED, QuantityId::DENSITY);
    ArrayView<const Size> neighCnt = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    parallelFor(0, r.size(), [&](const Size n1, const Size n2) INL {
        for (Size i = n1; i < n2; ++i) {
            // 'continuity equation' for smoothing lengths
            if (r[i][H] > 2._f * minimal) {
                v[i][H] = r[i][H] / dimensions * rhoDivv[i] / rho[i];
            } else {
                v[i][H] = 0._f;
            }

            /// \todo generalize for grad v
            // no acceleration of smoothing lengths (we evolve h as first-order quantity)
            dv[i][H] = 0._f;

            this->enforce(i, v, cs, neighCnt);
        }
    });
}

void StandardSph::AdaptiveSmoothingLength::create(Storage& storage, IMaterial& UNUSED(material)) const {
    storage.insert<Float>(QuantityId::DENSITY_VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
}

void BenzAsphaugSph::AdaptiveSmoothingLength::setDerivatives(DerivativeHolder& derivatives,
    const RunSettings& settings) {
    derivatives.require<VelocityDivergence>(settings);
}

void BenzAsphaugSph::AdaptiveSmoothingLength::finalize(Storage& storage) {
    ArrayView<const Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
    ArrayView<const Float> cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
    ArrayView<const Size> neighCnt = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
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

            this->enforce(i, v, cs, neighCnt);
        }
    });
}

void BenzAsphaugSph::AdaptiveSmoothingLength::create(Storage& storage, IMaterial& UNUSED(material)) const {
    storage.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
}

void ConstSmoothingLength::setDerivatives(DerivativeHolder& UNUSED(derivatives),
    const RunSettings& UNUSED(settings)) {}

void ConstSmoothingLength::initialize(Storage& UNUSED(storage)) {}

void ConstSmoothingLength::finalize(Storage& storage) {
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    parallelFor(0, r.size(), [&v, &dv](const Size n1, const Size n2) INL {
        for (Size i = n1; i < n2; ++i) {
            v[i][H] = 0._f;
            dv[i][H] = 0._f;
        }
    });
}

void ConstSmoothingLength::create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const {}

NAMESPACE_SPH_END
