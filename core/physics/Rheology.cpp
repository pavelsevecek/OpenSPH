#include "physics/Rheology.h"
#include "io/Logger.h"
#include "physics/Damage.h"
#include "quantities/IMaterial.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

// ----------------------------------------------------------------------------------------------------------
// VonMisesRheology
// ----------------------------------------------------------------------------------------------------------

VonMisesRheology::VonMisesRheology()
    : VonMisesRheology(makeAuto<NullFracture>()) {}

VonMisesRheology::VonMisesRheology(AutoPtr<IFractureModel>&& damage)
    : damage(std::move(damage)) {
    SPH_ASSERT(this->damage != nullptr);
}

VonMisesRheology::~VonMisesRheology() = default;

void VonMisesRheology::create(Storage& storage,
    IMaterial& material,
    const MaterialInitialContext& context) const {
    VERBOSE_LOG

    SPH_ASSERT(storage.getMaterialCnt() == 1);
    storage.insert<Float>(QuantityId::STRESS_REDUCING, OrderEnum::ZERO, 1._f);

    damage->setFlaws(storage, material, context);
}

void VonMisesRheology::initialize(IScheduler& scheduler, Storage& storage, const MaterialView material) {
    VERBOSE_LOG

    ArrayView<Float> u = storage.getValue<Float>(QuantityId::ENERGY);
    ArrayView<Float> reducing = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
    ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    ArrayView<TracelessTensor> S = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<Float> D;
    if (storage.has(QuantityId::DAMAGE)) {
        D = storage.getValue<Float>(QuantityId::DAMAGE);
    }

    const Float limit = material->getParam<Float>(BodySettingsId::ELASTICITY_LIMIT);
    SPH_ASSERT(limit > 0._f);

    const Float u_melt = material->getParam<Float>(BodySettingsId::MELT_ENERGY);
    IndexSequence seq = material.sequence();
    constexpr Float eps = 1.e-15_f;
    parallelFor(scheduler, *seq.begin(), *seq.end(), [&](const Size i) INL {
        // reduce the pressure (pressure is reduced only for negative values)
        const Float d = D ? pow<3>(D[i]) : 0._f;
        if (p[i] < 0._f) {
            p[i] = (1._f - d) * p[i];
        }

        // compute yielding stress
        const Float unorm = u[i] / u_melt;
        Float Y = unorm < 1.e-5_f ? limit : limit * max(1._f - unorm, 0._f);
        Y = (1._f - d) * Y;

        // apply reduction to stress tensor
        if (Y < EPS) {
            reducing[i] = 0._f;
            S[i] = TracelessTensor::null();
            return;
        }
        // compute second invariant using damaged stress tensor
        const Float J2 = 0.5_f * ddot(S[i], S[i]) + eps;
        SPH_ASSERT(isReal(J2) && J2 > 0._f);
        const Float red = min(Y / sqrt(3._f * J2), 1._f);
        SPH_ASSERT(red >= 0._f && red <= 1._f);
        reducing[i] = red;

        // apply yield reduction in place
        S[i] = S[i] * red;
        SPH_ASSERT(isReal(S[i]));
    });
}

void VonMisesRheology::integrate(IScheduler& scheduler, Storage& storage, const MaterialView material) {
    VERBOSE_LOG
    damage->integrate(scheduler, storage, material);
}

// ----------------------------------------------------------------------------------------------------------
// DruckerPragerRheology
// ----------------------------------------------------------------------------------------------------------

DruckerPragerRheology::DruckerPragerRheology()
    : DruckerPragerRheology(makeAuto<NullFracture>()) {}

DruckerPragerRheology::DruckerPragerRheology(AutoPtr<IFractureModel>&& damage)
    : damage(std::move(damage)) {
    SPH_ASSERT(this->damage != nullptr);
}

DruckerPragerRheology::~DruckerPragerRheology() = default;

void DruckerPragerRheology::create(Storage& storage,
    IMaterial& material,
    const MaterialInitialContext& context) const {
    VERBOSE_LOG
    SPH_ASSERT(storage.getMaterialCnt() == 1);
    storage.insert<Float>(QuantityId::STRESS_REDUCING, OrderEnum::ZERO, 1._f);
    if (material.getParam<bool>(BodySettingsId::USE_ACOUSTIC_FLUDIZATION)) {
        storage.insert<Float>(QuantityId::VIBRATIONAL_VELOCITY, OrderEnum::FIRST, 0._f);
        /// \todo user defined max value
        material.setRange(QuantityId::VIBRATIONAL_VELOCITY, Interval(0._f, LARGE), LARGE);
    }

    damage->setFlaws(storage, material, context);
}

void DruckerPragerRheology::initialize(IScheduler& scheduler, Storage& storage, const MaterialView material) {
    VERBOSE_LOG

    ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);
    ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    ArrayView<TracelessTensor> S = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<Float> reducing = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
    ArrayView<const Float> D;
    if (storage.has(QuantityId::DAMAGE)) {
        D = storage.getValue<Float>(QuantityId::DAMAGE);
    }

    const Float Y_0 = material->getParam<Float>(BodySettingsId::COHESION);
    const Float Y_M = material->getParam<Float>(BodySettingsId::ELASTICITY_LIMIT);
    const Float mu_i = material->getParam<Float>(BodySettingsId::INTERNAL_FRICTION);
    const Float mu_d = material->getParam<Float>(BodySettingsId::DRY_FRICTION);
    const Float u_melt = material->getParam<Float>(BodySettingsId::MELT_ENERGY);

    const bool fluidization = material->getParam<bool>(BodySettingsId::USE_ACOUSTIC_FLUDIZATION);
    // const Float nu_lim = material->getParam<Float>(BodySettingsId::FLUIDIZATION_VISCOSITY);
    ArrayView<const Float> v_vib, rho, cs;
    if (fluidization) {
        tie(v_vib, rho, cs) = storage.getValues<Float>(
            QuantityId::VIBRATIONAL_VELOCITY, QuantityId::DENSITY, QuantityId::SOUND_SPEED);
    }

    const IndexSequence seq = material.sequence();
    parallelFor(scheduler, *seq.begin(), *seq.end(), [&](const Size i) {
        // reduce the pressure (pressure is reduced only for negative values)
        const Float d = D ? pow<3>(D[i]) : 0._f;
        if (p[i] < 0._f) {
            p[i] = (1._f - d) * p[i];
        }

        const Float Y_i = Y_0 + mu_i * p[i] / (1._f + mu_i * max(p[i], 0._f) / (Y_M - Y_0));
        Float Y_d = mu_d * p[i];

        if (fluidization) {
            const Float p_vib = rho[i] * cs[i] * v_vib[i];
            Y_d = max(0._f, Y_d - mu_d * p_vib);
        }

        Float Y;
        if (Y_d > Y_i) {
            // at pressures above Y_i, the shear strength follows the same pressure dependence regardless
            // of damage.
            Y = Y_i;
        } else {
            Y = (1._f - d) * Y_i + d * Y_d;
        }

        // apply temperature dependence
        Y = (u[i] < 1.e-5_f * u_melt) ? Y : Y * max(1._f - u[i] / u_melt, 0._f);

        if (Y < EPS) {
            reducing[i] = 0._f;
            S[i] = TracelessTensor::null();
            return;
        }

        const Float J2 = 0.5_f * ddot(S[i], S[i]) + EPS;
        const Float red = min(Y / sqrt(J2), 1._f);
        SPH_ASSERT(red >= 0._f && red <= 1._f, red);
        reducing[i] = red;

        // apply yield reduction in place
        S[i] = S[i] * red;
        SPH_ASSERT(isReal(S[i]));
    });
}

void DruckerPragerRheology::integrate(IScheduler& scheduler, Storage& storage, const MaterialView material) {
    VERBOSE_LOG

    if (material->getParam<bool>(BodySettingsId::USE_ACOUSTIC_FLUDIZATION)) {
        // integrate the vibrational velocity
        ArrayView<Float> v, dv;
        tie(v, dv) = storage.getAll<Float>(QuantityId::VIBRATIONAL_VELOCITY);
        ArrayView<const SymmetricTensor> eps = storage.getValue<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT);
        ArrayView<const TracelessTensor> S = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
        ArrayView<const Float> p = storage.getValue<Float>(QuantityId::PRESSURE);

        const Float t_dec = material->getParam<Float>(BodySettingsId::OSCILLATION_DECAY_TIME);
        const Float e = material->getParam<Float>(BodySettingsId::OSCILLATION_REGENERATION);
        const Float rho = material->getParam<Float>(BodySettingsId::DENSITY);

        for (Size i : material.sequence()) {
            const SymmetricTensor sigma = p[i] * SymmetricTensor::identity() - SymmetricTensor(S[i]);
            const Float dE = e * max(ddot(sigma, eps[i]), 0._f);
            // energy to velocity
            dv[i] = sqrt(2._f * dE / rho) - v[i] / t_dec;
        }
    }

    damage->integrate(scheduler, storage, material);
}

// ----------------------------------------------------------------------------------------------------------
// ElasticRheology
// ----------------------------------------------------------------------------------------------------------

void ElasticRheology::create(Storage& storage,
    IMaterial& UNUSED(material),
    const MaterialInitialContext& UNUSED(context)) const {
    SPH_ASSERT(storage.getMaterialCnt() == 1);
    storage.insert<Float>(QuantityId::STRESS_REDUCING, OrderEnum::ZERO, 1._f);
}

void ElasticRheology::initialize(IScheduler& UNUSED(scheduler),
    Storage& UNUSED(storage),
    const MaterialView UNUSED(material)) {}

void ElasticRheology::integrate(IScheduler& UNUSED(scheduler),
    Storage& UNUSED(storage),
    const MaterialView UNUSED(material)) {}

// ----------------------------------------------------------------------------------------------------------
// DustRheology
// ----------------------------------------------------------------------------------------------------------

void DustRheology::create(Storage& UNUSED(storage),
    IMaterial& UNUSED(material),
    const MaterialInitialContext& UNUSED(context)) const {}

void DustRheology::initialize(IScheduler& UNUSED(scheduler),
    Storage& UNUSED(storage),
    const MaterialView UNUSED(material)) {}

void DustRheology::integrate(IScheduler& scheduler, Storage& storage, const MaterialView material) {
    const IndexSequence seq = material.sequence();
    ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    parallelFor(scheduler, *seq.begin(), *seq.end(), [&](const Size i) {
        p[i] = max(p[i], 0._f);
    });
}

NAMESPACE_SPH_END
