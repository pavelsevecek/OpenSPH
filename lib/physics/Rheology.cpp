#include "physics/Rheology.h"
#include "physics/Damage.h"
#include "quantities/AbstractMaterial.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

VonMisesRheology::VonMisesRheology()
    : VonMisesRheology(std::make_unique<NullDamage>()) {}

VonMisesRheology::VonMisesRheology(std::unique_ptr<Abstract::Damage>&& damage)
    : damage(std::move(damage)) {
    ASSERT(this->damage != nullptr);
}

VonMisesRheology::~VonMisesRheology() = default;

void VonMisesRheology::create(Storage& storage, const BodySettings& settings) const {
    ASSERT(storage.getMaterialCnt() == 1);
    storage.insert<Float>(QuantityId::YIELDING_REDUCE, OrderEnum::ZERO, 1._f);
    damage->setFlaws(storage, settings);
}

void VonMisesRheology::initialize(Storage& storage, const MaterialView material) {
    damage->reduce(storage, material);
    ArrayView<Float> u = storage.getValue<Float>(QuantityId::ENERGY);
    ArrayView<Float> reducing = storage.getValue<Float>(QuantityId::YIELDING_REDUCE);
    ArrayView<TracelessTensor> S = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<Float> D;
    if (storage.has(QuantityId::DAMAGE)) {
        D = storage.getValue<Float>(QuantityId::DAMAGE);
    }

    const Float limit = material->getParam<Float>(BodySettingsId::ELASTICITY_LIMIT);
    const Float u_melt = material->getParam<Float>(BodySettingsId::MELT_ENERGY);
    for (Size i : material.sequence()) {
        // compute yielding stress

        const Float y = limit * max(1.f - u[i] / u_melt, 0._f);
        ASSERT(limit > 0._f);

        // apply reduction to stress tensor
        if (y < EPS) {
            reducing[i] = 0._f;
            S[i] = TracelessTensor::null();
            continue;
        }
        TracelessTensor s;
        if (D) {
            const Float d = pow<3>(D[i]);
            s = (1._f - d) * S[i];
        } else {
            s = S[i];
        }
        const Float inv = 0.5_f * ddot(s, s) / sqr(y) + EPS;
        ASSERT(isReal(inv) && inv > 0._f);
        const Float red = min(sqrt(1._f / (3._f * inv)), 1._f);
        ASSERT(red >= 0._f && red <= 1._f);
        reducing[i] = red;
        S[i] = S[i] * red;
        ASSERT(isReal(S[i]));
    }
}

void VonMisesRheology::integrate(Storage& storage, const MaterialView material) {
    damage->integrate(storage, material);
}

DruckerPragerRheology::DruckerPragerRheology(std::unique_ptr<Abstract::Damage>&& damage)
    : damage(std::move(damage)) {
    if (this->damage == nullptr) {
        this->damage = std::make_unique<NullDamage>();
    }
}

DruckerPragerRheology::~DruckerPragerRheology() = default;

void DruckerPragerRheology::create(Storage& storage, const BodySettings& settings) const {
    ASSERT(storage.getMaterialCnt() == 1);
    /// \todo implement, this is copy+paste of von mises
    storage.insert<Float>(QuantityId::YIELDING_REDUCE, OrderEnum::ZERO, 1._f);
    damage->setFlaws(storage, settings);
}

void DruckerPragerRheology::initialize(Storage& storage, const MaterialView material) {
    yieldingStress.clear();
    /// ArrayView<Float> u = storage.getValue<Float>(QuantityId::ENERGY); \todo dependence on melt energy
    ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    ArrayView<Float> D = storage.getValue<Float>(QuantityId::DAMAGE);
    for (Size i : material.sequence()) {
        const Float Y_0 = material->getParam<Float>(BodySettingsId::COHESION);
        const Float mu_i = material->getParam<Float>(BodySettingsId::INTERNAL_FRICTION);
        const Float Y_M = material->getParam<Float>(BodySettingsId::ELASTICITY_LIMIT);
        // const Float u_melt = storage.getParam<Float>(BodySettingsId::MELT_ENERGY);
        const Float Y_i = Y_0 + mu_i * p[i] / (1._f + mu_i * p[i] / (Y_M - Y_0));
        ASSERT(Y_i >= 0);

        const Float mu_d = material->getParam<Float>(BodySettingsId::DRY_FRICTION);
        const Float Y_d = mu_d * p[i];
        if (Y_d > Y_i) {
            // at pressures above Y_i, the shear strength follows the same pressure dependence regardless
            // of damage.
            yieldingStress.push(Y_i);
        } else {
            const Float d = pow<3>(D[i]);
            const Float Y = (1._f - d) * Y_i + d * Y_d;
            yieldingStress.push(Y);
        }
    }
}

void DruckerPragerRheology::integrate(Storage& storage, const MaterialView material) {
    damage->integrate(storage, material);
}

NAMESPACE_SPH_END
