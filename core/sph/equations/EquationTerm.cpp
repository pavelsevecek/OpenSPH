#include "sph/equations/EquationTerm.h"
#include "objects/Exceptions.h"
#include "sph/Materials.h"
#include "sph/equations/DerivativeHelpers.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

/// \brief Discretization of pressure term in standard SPH formulation.
class StandardForceDiscr {
    ArrayView<const Float> rho;

public:
    void initialize(const Storage& input) {
        rho = input.getValue<Float>(QuantityId::DENSITY);
    }

    template <typename T>
    INLINE T eval(const Size i, const Size j, const T& vi, const T& vj) const {
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
    INLINE T eval(const Size i, const Size j, const T& vi, const T& vj) const {
        return (vi + vj) / (rho[i] * rho[j]);
    }
};

template <typename Discr>
class PressureGradient : public AccelerationTemplate<PressureGradient<Discr>> {
private:
    ArrayView<const Float> p;
    Discr discr;

public:
    using AccelerationTemplate<PressureGradient<Discr>>::AccelerationTemplate;

    INLINE void additionalCreate(Accumulated& UNUSED(results)) {}

    INLINE void additionalInitialize(const Storage& input, Accumulated& UNUSED(results)) {
        p = input.getValue<Float>(QuantityId::PRESSURE);
        discr.initialize(input);
    }

    INLINE bool additionalEquals(const PressureGradient& UNUSED(other)) const {
        return true;
    }

    template <bool Symmetric>
    INLINE Tuple<Vector, Float> eval(const Size i, const Size j, const Vector& grad) {
        const Vector f = discr.eval(i, j, p[i], p[j]) * grad;
        SPH_ASSERT(isReal(f));
        return { -f, 0._f };
    }
};

void PressureForce::setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) {
    const DiscretizationEnum formulation =
        settings.get<DiscretizationEnum>(RunSettingsId::SPH_DISCRETIZATION);
    switch (formulation) {
    case DiscretizationEnum::STANDARD:
        derivatives.require(makeAuto<VelocityDivergence<CenterDensityDiscr>>(settings));
        derivatives.require(makeAuto<PressureGradient<StandardForceDiscr>>(settings));
        break;
    case DiscretizationEnum::BENZ_ASPHAUG:
        derivatives.require(makeAuto<VelocityDivergence<NeighborDensityDiscr>>(settings));
        derivatives.require(makeAuto<PressureGradient<BenzAsphaugForceDiscr>>(settings));
        break;
    default:
        NOT_IMPLEMENTED;
    }
}

void PressureForce::initialize(IScheduler& UNUSED(scheduler),
    Storage& UNUSED(storage),
    const Float UNUSED(t)) {}

void PressureForce::finalize(IScheduler& scheduler, Storage& storage, const Float UNUSED(t)) {
    ArrayView<const Float> p, rho;
    tie(p, rho) = storage.getValues<Float>(QuantityId::PRESSURE, QuantityId::DENSITY);
    ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);
    ArrayView<const Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
    parallelFor(scheduler, 0, du.size(), [&](const Size i) INL { //
        du[i] -= p[i] / rho[i] * divv[i];
        SPH_ASSERT(isReal(du[i]));
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
class StressDivergence : public AccelerationTemplate<StressDivergence<Discr>> {
private:
    ArrayView<const TracelessTensor> s;
    Discr discr;

public:
    explicit StressDivergence(const RunSettings& settings)
        : AccelerationTemplate<StressDivergence<Discr>>(settings, DerivativeFlag::SUM_ONLY_UNDAMAGED) {}

    INLINE void additionalCreate(Accumulated& UNUSED(results)) {}

    INLINE void additionalInitialize(const Storage& input, Accumulated& UNUSED(results)) {
        s = input.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
        discr.initialize(input);
    }

    INLINE bool additionalEquals(const StressDivergence& UNUSED(other)) const {
        return true;
    }

    template <bool Symmetrize>
    INLINE Tuple<Vector, Float> eval(const Size i, const Size j, const Vector& grad) {
        const Vector f = discr.eval(i, j, s[i], s[j]) * grad;
        SPH_ASSERT(isReal(f));
        return { f, 0._f };
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
        derivatives.require(makeAuto<CorrectionTensor>(settings));
    }

    const DiscretizationEnum formulation =
        settings.get<DiscretizationEnum>(RunSettingsId::SPH_DISCRETIZATION);
    switch (formulation) {
    case DiscretizationEnum::STANDARD:
        derivatives.require(makeAuto<StressDivergence<StandardForceDiscr>>(settings));
        break;
    case DiscretizationEnum::BENZ_ASPHAUG:
        derivatives.require(makeAuto<StressDivergence<BenzAsphaugForceDiscr>>(settings));
        break;
    default:
        NOT_IMPLEMENTED;
    }
}

void SolidStressForce::initialize(IScheduler& UNUSED(scheduler),
    Storage& UNUSED(storage),
    const Float UNUSED(t)) {}

void SolidStressForce::finalize(IScheduler& scheduler, Storage& storage, const Float UNUSED(t)) {
    ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    ArrayView<TracelessTensor> s, ds;
    tie(s, ds) = storage.getAll<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);
    ArrayView<SymmetricTensor> gradv = storage.getValue<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT);

    for (Size matIdx = 0; matIdx < storage.getMaterialCnt(); ++matIdx) {
        MaterialView material = storage.getMaterial(matIdx);
        const YieldingEnum yield = material->getParam<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
        if (yield == YieldingEnum::NONE || yield == YieldingEnum::DUST) {
            // no rheology, do not integrate stress tensor
            continue;
        }
        /// \todo add to rheology?
        const Float mu = material->getParam<Float>(BodySettingsId::SHEAR_MODULUS);
        IndexSequence seq = material.sequence();
        parallelFor(scheduler, *seq.begin(), *seq.end(), [&](const Size i) INL {
            du[i] += 1._f / rho[i] * ddot(s[i], gradv[i]);

            // Hooke's law
            TracelessTensor dev(gradv[i] - SymmetricTensor::identity() * gradv[i].trace() / 3._f);
            ds[i] += 2._f * mu * dev;
            SPH_ASSERT(isReal(du[i]) && isReal(ds[i]));
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

void NavierStokesForce::initialize(IScheduler& UNUSED(scheduler),
    Storage& UNUSED(storage),
    const Float UNUSED(t)) {
    TODO("implement");
}

void NavierStokesForce::finalize(IScheduler& UNUSED(scheduler), Storage& storage, const Float UNUSED(t)) {
    ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    ArrayView<TracelessTensor> s, ds;
    tie(s, ds) = storage.getAll<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
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
            SPH_ASSERT(isReal(du[i]) && isReal(ds[i]));
        }
    }
}

void NavierStokesForce::create(Storage& storage, IMaterial& material) const {
    SPH_ASSERT(storage.has(QuantityId::ENERGY) && storage.has(QuantityId::PRESSURE));
    storage.insert<TracelessTensor>(QuantityId::DEVIATORIC_STRESS,
        OrderEnum::ZERO,
        material.getParam<TracelessTensor>(BodySettingsId::STRESS_TENSOR));
}


ContinuityEquation::ContinuityEquation(const RunSettings& settings) {
    mode = settings.get<ContinuityEnum>(RunSettingsId::SPH_CONTINUITY_MODE);

    LutKernel<3> kernel = Factory::getKernel<3>(settings);
    // central value of the kernel
    w0 = kernel.valueImpl(0);
}

void ContinuityEquation::setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) {
    // this formulation uses equation \dot \rho_i = m_i \sum_j m_j/rho_j \nabla \cdot \vec  v where the
    // velocity divergence is taken either directly, or as a trace of strength velocity gradient, see
    // below.
    Flags<ForceEnum> forces = settings.getFlags<ForceEnum>(RunSettingsId::SPH_SOLVER_FORCES);
    if (forces.has(ForceEnum::SOLID_STRESS)) {
        const Flags<DerivativeFlag> flags = DerivativeFlag::CORRECTED | DerivativeFlag::SUM_ONLY_UNDAMAGED;
        derivatives.require(makeDerivative<VelocityGradient>(settings, flags));
    } else if (mode == ContinuityEnum::SUM_ONLY_UNDAMAGED) {
        throw InvalidSetup("This mode of the continuity equation requires stress tensor.");
    }
    derivatives.require(makeDerivative<VelocityDivergence>(settings));
}

void ContinuityEquation::initialize(IScheduler& UNUSED(scheduler),
    Storage& UNUSED(storage),
    const Float UNUSED(t)) {}

void ContinuityEquation::finalize(IScheduler& scheduler, Storage& storage, const Float UNUSED(t)) {
    ArrayView<Float> rho, drho;
    tie(rho, drho) = storage.getAll<Float>(QuantityId::DENSITY);
    ArrayView<const Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);

    switch (mode) {
    case ContinuityEnum::STANDARD:
        parallelFor(scheduler, 0, rho.size(), [&](const Size i) INL { //
            drho[i] += -rho[i] * divv[i];
        });
        break;
    case ContinuityEnum::SUM_ONLY_UNDAMAGED: {
        ArrayView<const Float> reduce = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
        ArrayView<const SymmetricTensor> gradv =
            storage.getValue<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT);
        parallelFor(scheduler, 0, rho.size(), [&](const Size i) INL {
            if (reduce[i] > 0._f) {
                drho[i] += -rho[i] * gradv[i].trace();
            } else {
                drho[i] += -rho[i] * divv[i];
            }
        });
        break;
    }
    default:
        NOT_IMPLEMENTED;
    }
}

void ContinuityEquation::create(Storage& storage, IMaterial& material) const {
    const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
    storage.insert<Float>(QuantityId::DENSITY, OrderEnum::FIRST, rho0);

    // set minimal density based on masses and smoothing kernel
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    Float rhoLimit = LARGE;
    for (Size i = 0; i < r.size(); ++i) {
        rhoLimit = min(rhoLimit, m[i] * w0 / pow<3>(r[i][H]));
    }
    const Interval rhoRange = material.getParam<Interval>(BodySettingsId::DENSITY_RANGE);
    const Float rhoSmall = material.getParam<Float>(BodySettingsId::DENSITY_MIN);
    const Float rho_min = max(rhoLimit, rhoRange.lower());
    const Float rho_max = rhoRange.upper();
    material.setRange(QuantityId::DENSITY, Interval(rho_min, rho_max), rhoSmall);

    storage.insert<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO, 0._f);
}


AdaptiveSmoothingLength::AdaptiveSmoothingLength(const RunSettings& settings, const Size dimensions)
    : dimensions(dimensions) {
    Flags<SmoothingLengthEnum> flags =
        settings.getFlags<SmoothingLengthEnum>(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH);
    if (flags.has(SmoothingLengthEnum::SOUND_SPEED_ENFORCING)) {
        enforcing.strength = settings.get<Float>(RunSettingsId::SPH_NEIGHBOR_ENFORCING);
        enforcing.range = settings.get<Interval>(RunSettingsId::SPH_NEIGHBOR_RANGE);
    } else {
        enforcing.strength = -INFTY;
    }
    range = settings.get<Interval>(RunSettingsId::SPH_SMOOTHING_LENGTH_RANGE);
}

void AdaptiveSmoothingLength::setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) {
    derivatives.require(makeDerivative<VelocityDivergence>(settings));
}

void AdaptiveSmoothingLength::initialize(IScheduler& UNUSED(scheduler),
    Storage& storage,
    const Float UNUSED(t)) {
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    // clamp smoothing lengths
    for (Size i = 0; i < r.size(); ++i) {
        r[i][H] = range.clamp(r[i][H]);
    }
}

void AdaptiveSmoothingLength::finalize(IScheduler& scheduler, Storage& storage, const Float UNUSED(t)) {
    ArrayView<const Float> divv, cs;
    tie(divv, cs) = storage.getValues<Float>(QuantityId::VELOCITY_DIVERGENCE, QuantityId::SOUND_SPEED);
    ArrayView<const Size> neighCnt = storage.getValue<Size>(QuantityId::NEIGHBOR_CNT);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

    parallelFor(scheduler, 0, r.size(), [&](const Size i) INL {
        // 'continuity equation' for smoothing lengths
        if (r[i][H] > 2._f * range.lower()) {
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

    // check upper limit of neighbor count
    const Float dn1 = neighCnt[i] - enforcing.range.upper();
    SPH_ASSERT(dn1 < neighCnt.size());
    if (dn1 > 0._f) {
        // sound speed is used to add correct dimensions to the term
        v[i][H] -= exp(enforcing.strength * dn1) * cs[i];
        return;
    }
    // check lower limit of neighbor count
    const Float dn2 = enforcing.range.lower() - neighCnt[i];
    SPH_ASSERT(dn2 < neighCnt.size());
    if (dn2 > 0._f) {
        v[i][H] += exp(enforcing.strength * dn2) * cs[i];
    }

    SPH_ASSERT(isReal(v[i]));
}

void ConstSmoothingLength::setDerivatives(DerivativeHolder& UNUSED(derivatives),
    const RunSettings& UNUSED(settings)) {}

void ConstSmoothingLength::initialize(IScheduler& UNUSED(scheduler),
    Storage& UNUSED(storage),
    const Float UNUSED(t)) {}

void ConstSmoothingLength::finalize(IScheduler& scheduler, Storage& storage, const Float UNUSED(t)) {
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    parallelFor(scheduler, 0, r.size(), [&v, &dv](const Size i) INL {
        v[i][H] = 0._f;
        dv[i][H] = 0._f;
    });
}

void ConstSmoothingLength::create(Storage& UNUSED(storage), IMaterial& UNUSED(material)) const {}

NAMESPACE_SPH_END
