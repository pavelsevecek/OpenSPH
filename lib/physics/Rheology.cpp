#include "physics/Rheology.h"
#include "physics/Damage.h"
#include "quantities/IMaterial.h"
#include "quantities/Storage.h"
#include "thread/Pool.h"

NAMESPACE_SPH_BEGIN

/// ----------------------------------------------------------------------------------------------------------
/// VonMisesRheology
/// ----------------------------------------------------------------------------------------------------------

VonMisesRheology::VonMisesRheology()
    : VonMisesRheology(makeAuto<NullFracture>()) {}

VonMisesRheology::VonMisesRheology(AutoPtr<IFractureModel>&& damage)
    : damage(std::move(damage)) {
    ASSERT(this->damage != nullptr);
}

VonMisesRheology::~VonMisesRheology() = default;

void VonMisesRheology::create(Storage& storage,
    IMaterial& material,
    const MaterialInitialContext& context) const {
    ASSERT(storage.getMaterialCnt() == 1);
    storage.insert<Float>(QuantityId::STRESS_REDUCING, OrderEnum::ZERO, 1._f);

    damage->setFlaws(storage, material, context);
}

void VonMisesRheology::initialize(IScheduler& scheduler, Storage& storage, const MaterialView material) {
    ArrayView<Float> u = storage.getValue<Float>(QuantityId::ENERGY);
    ArrayView<Float> reducing = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
    ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    ArrayView<TracelessTensor> S = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<Float> D;
    if (storage.has(QuantityId::DAMAGE)) {
        D = storage.getValue<Float>(QuantityId::DAMAGE);
    }

    const Float limit = material->getParam<Float>(BodySettingsId::ELASTICITY_LIMIT);
    ASSERT(limit > 0._f);

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
        Float Y = unorm < 1.e-5_f ? limit : limit * max(1.f - unorm, 0._f);
        Y = (1._f - d) * Y;

        // apply reduction to stress tensor
        if (Y < EPS) {
            reducing[i] = 0._f;
            S[i] = TracelessTensor::null();
            return;
        }
        // compute second invariant using damaged stress tensor
        const Float J2 = 0.5_f * ddot(S[i], S[i]) + eps;
        ASSERT(isReal(J2) && J2 > 0._f);
        const Float red = min(Y / sqrt(3._f * J2), 1._f);
        ASSERT(red >= 0._f && red <= 1._f);
        reducing[i] = red;

        // apply yield reduction in place
        S[i] = S[i] * red;
        ASSERT(isReal(S[i]));
    });
}

void VonMisesRheology::integrate(IScheduler& scheduler, Storage& storage, const MaterialView material) {
    damage->integrate(scheduler, storage, material);
}

/// ----------------------------------------------------------------------------------------------------------
/// DruckerPragerRheology
/// ----------------------------------------------------------------------------------------------------------

DruckerPragerRheology::DruckerPragerRheology()
    : DruckerPragerRheology(makeAuto<NullFracture>()) {}

DruckerPragerRheology::DruckerPragerRheology(AutoPtr<IFractureModel>&& damage)
    : damage(std::move(damage)) {
    ASSERT(this->damage != nullptr);
}

DruckerPragerRheology::~DruckerPragerRheology() = default;

void DruckerPragerRheology::create(Storage& storage,
    IMaterial& material,
    const MaterialInitialContext& context) const {
    ASSERT(storage.getMaterialCnt() == 1);
    storage.insert<Float>(QuantityId::STRESS_REDUCING, OrderEnum::ZERO, 1._f);
    damage->setFlaws(storage, material, context);
}

void DruckerPragerRheology::initialize(IScheduler& scheduler, Storage& storage, const MaterialView material) {
    ArrayView<Float> u = storage.getValue<Float>(QuantityId::ENERGY);
    ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    ArrayView<TracelessTensor> S = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<Float> reducing = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
    ArrayView<Float> D;
    if (storage.has(QuantityId::DAMAGE)) {
        D = storage.getValue<Float>(QuantityId::DAMAGE);
    }

    const Float Y_0 = material->getParam<Float>(BodySettingsId::COHESION);
    const Float Y_M = material->getParam<Float>(BodySettingsId::ELASTICITY_LIMIT);
    const Float mu_i = material->getParam<Float>(BodySettingsId::INTERNAL_FRICTION);
    const Float mu_d = material->getParam<Float>(BodySettingsId::DRY_FRICTION);
    const Float u_melt = material->getParam<Float>(BodySettingsId::MELT_ENERGY);

    const IndexSequence seq = material.sequence();
    parallelFor(scheduler, *seq.begin(), *seq.end(), [&](const Size i) {
        // reduce the pressure (pressure is reduced only for negative values)
        const Float d = D ? pow<3>(D[i]) : 0._f;
        if (p[i] < 0._f) {
            p[i] = (1._f - d) * p[i];
        }

        const Float Y_i = Y_0 + mu_i * p[i] / (1._f + mu_i * max(p[i], 0._f) / (Y_M - Y_0));
        const Float Y_d = mu_d * p[i];

        Float Y;
        if (Y_d > Y_i) {
            // at pressures above Y_i, the shear strength follows the same pressure dependence regardless
            // of damage.
            Y = Y_i;
        } else {
            Y = (1._f - d) * Y_i + d * Y_d;
        }

        // apply temperature dependence
        Y = (u[i] < 1.e-5_f * u_melt) ? Y : Y * max(1.f - u[i] / u_melt, 0._f);

        if (Y < EPS) {
            reducing[i] = 0._f;
            S[i] = TracelessTensor::null();
            return;
        }

        const Float J2 = 0.5_f * ddot(S[i], S[i]) + EPS;
        const Float red = min(Y / sqrt(J2), 1._f);
        ASSERT(red >= 0._f && red <= 1._f, red);
        reducing[i] = red;

        // apply yield reduction in place
        S[i] = S[i] * red;
        ASSERT(isReal(S[i]));
    });
}

void DruckerPragerRheology::integrate(IScheduler& scheduler, Storage& storage, const MaterialView material) {
    damage->integrate(scheduler, storage, material);
}

void ElasticRheology::create(Storage& storage,
    IMaterial& UNUSED(material),
    const MaterialInitialContext& UNUSED(context)) const {
    ASSERT(storage.getMaterialCnt() == 1);
    storage.insert<Float>(QuantityId::STRESS_REDUCING, OrderEnum::ZERO, 1._f);
}

void ElasticRheology::initialize(IScheduler& UNUSED(scheduler),
    Storage& UNUSED(storage),
    const MaterialView UNUSED(material)) {}

void ElasticRheology::integrate(IScheduler& UNUSED(scheduler),
    Storage& UNUSED(storage),
    const MaterialView UNUSED(material)) {}

NAMESPACE_SPH_END
