#include "sph/equations/EquationTerm.h"

NAMESPACE_SPH_BEGIN

template <typename Discr>
class PressureGradient : public DerivativeTemplate<PressureGradient<Discr>> {
private:
    ArrayView<const Float> p, m;
    Discr discr;

    ArrayView<Vector> dv;

public:
    using DerivativeTemplate<PressureGradient<Discr>>::DerivativeTemplate;

    virtual void create(Accumulated& results) override {
        results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, BufferSource::SHARED);
    }

    INLINE void init(const Storage& input, Accumulated& results) {
        tie(p, m) = input.getValues<Float>(QuantityId::PRESSURE, QuantityId::MASS);
        discr.initialize(input);

        dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        const Vector f = discr.eval(i, j, p[i], p[j]) * grad;
        ASSERT(isReal(f));
        dv[i] -= m[j] * f;
        if (Symmetrize) {
            dv[j] += m[i] * f;
        }
    }
};

void PressureForce::setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) {
    const FormulationEnum formulation = settings.get<FormulationEnum>(RunSettingsId::SPH_FORMULATION);
    switch (formulation) {
    case FormulationEnum::STANDARD:
        derivatives.require(makeAuto<VelocityDivergence<CenterDensityDiscr>>(settings));
        derivatives.require(makeAuto<PressureGradient<StandardForceDiscr>>(settings));
        break;
    case FormulationEnum::BENZ_ASPHAUG:
        derivatives.require(makeAuto<VelocityDivergence<NeighbourDensityDiscr>>(settings));
        derivatives.require(makeAuto<PressureGradient<BenzAsphaugForceDiscr>>(settings));
        break;
    default:
        NOT_IMPLEMENTED;
    }
}

void PressureForce::initialize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) {}

void PressureForce::finalize(IScheduler& scheduler, Storage& storage) {
    ArrayView<const Float> p, rho;
    tie(p, rho) = storage.getValues<Float>(QuantityId::PRESSURE, QuantityId::DENSITY);
    ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);
    ArrayView<const Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
    parallelFor(scheduler, 0, du.size(), [&](const Size i) INL { //
        du[i] -= p[i] / rho[i] * divv[i];
    });
}

void PressureForce::create(Storage& storage, IMaterial& material) const {
    if (!dynamic_cast<EosMaterial*>(&material)) {
        throw InvalidSetup("PressureForce needs to be used with EosMaterial or derived");
    }
    const Float u0 = material.getParam<Float>(BodySettingsId::ENERGY);
    storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, u0);
    material.setRange(QuantityId::ENERGY, BodySettingsId::ENERGY_RANGE, BodySettingsId::ENERGY_MIN);
    // need to create quantity for velocity divergence so that we can save it to storage later
    storage.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
}


template <typename Discr>
class StressDivergence : public DerivativeTemplate<StressDivergence<Discr>> {
private:
    ArrayView<const Float> m;
    ArrayView<const TracelessTensor> s;
    Discr discr;

    ArrayView<Vector> dv;

public:
    explicit StressDivergence(const RunSettings& settings)
        : DerivativeTemplate<StressDivergence<Discr>>(settings, DerivativeFlag::SUM_ONLY_UNDAMAGED) {}

    virtual void create(Accumulated& results) override {
        results.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, BufferSource::SHARED);
    }

    INLINE void init(const Storage& input, Accumulated& results) {
        m = input.getValue<Float>(QuantityId::MASS);
        s = input.getPhysicalValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
        discr.initialize(input);

        dv = results.getBuffer<Vector>(QuantityId::POSITION, OrderEnum::SECOND);
    }

    template <bool Symmetrize>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        const Vector f = discr.eval(i, j, s[i], s[j]) * grad;
        ASSERT(isReal(f));
        dv[i] += m[j] * f;
        if (Symmetrize) {
            dv[j] -= m[i] * f;
        }
    }
};

SolidStressForce::SolidStressForce(const RunSettings& settings) {
    // the correction tensor is associated with velocity gradient, which we are creating in this term, so we
    // need to also create the correction tensor (if requested)
    /// \todo correction tensor is now generalized, it is easily accessible to ALL derivatives deriving from
    /// DerivativeTemplate. We should create this quantity somehow automatically, without relying on
    /// SolidStressForce to add it.
    useCorrectionTensor = settings.get<bool>(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR);
}

void SolidStressForce::setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) {
    derivatives.require(makeDerivative<VelocityGradient>(
        settings, DerivativeFlag::SUM_ONLY_UNDAMAGED | DerivativeFlag::CORRECTED));
    /// \todo Correction tensor should be added automatically; same as above
    if (useCorrectionTensor) {
        derivatives.require(makeAuto<CorrectionTensor>());
    }

    const FormulationEnum formulation = settings.get<FormulationEnum>(RunSettingsId::SPH_FORMULATION);
    switch (formulation) {
    case FormulationEnum::STANDARD:
        derivatives.require(makeAuto<StressDivergence<StandardForceDiscr>>(settings));
        break;
    case FormulationEnum::BENZ_ASPHAUG:
        derivatives.require(makeAuto<StressDivergence<BenzAsphaugForceDiscr>>(settings));
        break;
    default:
        NOT_IMPLEMENTED;
    }
}

void SolidStressForce::initialize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) {}

void SolidStressForce::finalize(IScheduler& scheduler, Storage& storage) {
    ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    ArrayView<TracelessTensor> s, ds;
    tie(s, ds) = storage.getPhysicalAll<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);
    ArrayView<SymmetricTensor> gradv = storage.getValue<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT);
    // ArrayView<Vector> rhoRotV = storage.getValue<Vector>(QuantityId::STRENGTH_DENSITY_VELOCITY_ROTATION);
    ArrayView<Float> reduce = storage.getValue<Float>(QuantityId::STRESS_REDUCING);

    for (Size matIdx = 0; matIdx < storage.getMaterialCnt(); ++matIdx) {
        MaterialView material = storage.getMaterial(matIdx);
        const Float mu = material->getParam<Float>(BodySettingsId::SHEAR_MODULUS);
        IndexSequence seq = material.sequence();
        parallelFor(scheduler, *seq.begin(), *seq.end(), [&](const Size i) INL {
            if (reduce[i] == 0._f) {
                return;
            }
            du[i] += 1._f / rho[i] * ddot(s[i], gradv[i]);

            // Hooke's law
            TracelessTensor dev(gradv[i] - SymmetricTensor::identity() * gradv[i].trace() / 3._f);
            ds[i] += 2._f * mu * dev;

            // add rotation terms for independence of reference frame
            /*const AffineMatrix R = 0.5_f * AffineMatrix::crossProductOperator(rhoRotV[i]);
            const AffineMatrix S = convert<AffineMatrix>(s[i]);
            const TracelessTensor ds_rot = convert<TracelessTensor>(1._f / rho[i] * (S * R - R * S));
            ds[i] += ds_rot;*/
            ASSERT(isReal(du[i]) && isReal(ds[i]));
        });
    }
}

void SolidStressForce::create(Storage& storage, IMaterial& material) const {
    const TracelessTensor s0 = material.getParam<TracelessTensor>(BodySettingsId::STRESS_TENSOR);
    storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::FIRST, s0);
    const Float s_min = material.getParam<Float>(BodySettingsId::STRESS_TENSOR_MIN);
    material.setRange(QuantityId::DEVIATORIC_STRESS, Interval::unbounded(), s_min);

    /// \todo make sure that there is only a single derivative writing to VELOCITY_GRADIENT!!
    storage.insert<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT, OrderEnum::ZERO, SymmetricTensor::null());
    // storage.insert<Vector>(QuantityId::VELOCITY_ROTATION, OrderEnum::ZERO, Vector(0._f));

    if (useCorrectionTensor) {
        storage.insert<SymmetricTensor>(
            QuantityId::STRAIN_RATE_CORRECTION_TENSOR, OrderEnum::ZERO, SymmetricTensor::identity());
    }
}

void NavierStokesForce::setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) {
    /// derivatives.require<StressDivergence>(derivatives, settings);
    TODO("implement");
    // no need to do 'hacks' with gradient for fluids
    derivatives.require(makeDerivative<VelocityGradient>(settings, DerivativeFlag::CORRECTED));
}

void NavierStokesForce::initialize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) {
    TODO("implement");
}

void NavierStokesForce::finalize(IScheduler& UNUSED(scheduler), Storage& storage) {
    ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    ArrayView<TracelessTensor> s, ds;
    tie(s, ds) = storage.getPhysicalAll<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);
    ArrayView<SymmetricTensor> gradv = storage.getValue<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT);

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

void NavierStokesForce::create(Storage& storage, IMaterial& material) const {
    ASSERT(storage.has(QuantityId::ENERGY) && storage.has(QuantityId::PRESSURE));
    storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS,
        OrderEnum::ZERO,
        material.getParam<TracelessTensor>(BodySettingsId::STRESS_TENSOR));
}


void ContinuityEquation::setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) {
    // this formulation uses equation \dot \rho_i = m_i \sum_j m_j/rho_j \nabla \cdot \vec  v where the
    // velocity divergence is taken either directly, or as a trace of strength velocity gradient, see
    // below.
    Flags<ForceEnum> forces = settings.getFlags<ForceEnum>(RunSettingsId::SOLVER_FORCES);
    if (forces.has(ForceEnum::SOLID_STRESS)) {
        const Flags<DerivativeFlag> flags = DerivativeFlag::CORRECTED | DerivativeFlag::SUM_ONLY_UNDAMAGED;
        derivatives.require(makeDerivative<VelocityGradient>(settings, flags));
    }
    derivatives.require(makeDerivative<VelocityDivergence>(settings));
}

void ContinuityEquation::initialize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) {}

void ContinuityEquation::finalize(IScheduler& scheduler, Storage& storage) {
    ArrayView<Float> rho, drho;
    tie(rho, drho) = storage.getAll<Float>(QuantityId::DENSITY);
    ArrayView<const Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);

    if (storage.has(QuantityId::VELOCITY_GRADIENT)) {
        ArrayView<const Float> reduce = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
        ArrayView<const SymmetricTensor> gradv =
            storage.getValue<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT);
        parallelFor(scheduler, 0, rho.size(), [&](const Size i) INL {
            if (reduce[i] > 0._f) {
                drho[i] = -rho[i] * gradv[i].trace();
            } else {
                drho[i] = -rho[i] * divv[i];
            }
        });
    } else {
        parallelFor(scheduler, 0, rho.size(), [&](const Size i) INL { //
            drho[i] = -rho[i] * divv[i];
        });
    }
}

void ContinuityEquation::create(Storage& storage, IMaterial& material) const {
    const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, rho0);
    material.setRange(QuantityId::DENSITY, BodySettingsId::DENSITY_RANGE, BodySettingsId::DENSITY_MIN);

    storage.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
}


AdaptiveSmoothingLength::AdaptiveSmoothingLength(const RunSettings& settings, const Size dimensions)
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

void AdaptiveSmoothingLength::setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) {
    derivatives.require(makeDerivative<VelocityDivergence>(settings));
}

void AdaptiveSmoothingLength::initialize(IScheduler& UNUSED(scheduler), Storage& storage) {
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    // clamp smoothing lengths
    for (Size i = 0; i < r.size(); ++i) {
        r[i][H] = max(r[i][H], minimal);
    }
}

void AdaptiveSmoothingLength::finalize(IScheduler& scheduler, Storage& storage) {
    ArrayView<const Float> divv, cs;
    tie(divv, cs) = storage.getValues<Float>(QuantityId::VELOCITY_DIVERGENCE, QuantityId::SOUND_SPEED);
    ArrayView<const Size> neighCnt = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

    parallelFor(scheduler, 0, r.size(), [&](const Size i) INL {
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
    });
}

void AdaptiveSmoothingLength::create(Storage& storage, IMaterial& UNUSED(material)) const {
    storage.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
}

INLINE void AdaptiveSmoothingLength::enforce(const Size i,
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

void ConstSmoothingLength::setDerivatives(DerivativeHolder& UNUSED(derivatives),
    const RunSettings& UNUSED(settings)) {}

void ConstSmoothingLength::initialize(IScheduler& UNUSED(scheduler), Storage& UNUSED(storage)) {}

void ConstSmoothingLength::finalize(IScheduler& scheduler, Storage& storage) {
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    parallelFor(scheduler, 0, r.size(), [&v, &dv](const Size i) INL {
        v[i][H] = 0._f;
        dv[i][H] = 0._f;
    });
}

void ConstSmoothingLength::create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const {}

NAMESPACE_SPH_END
