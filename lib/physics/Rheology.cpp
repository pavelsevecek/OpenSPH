#include "physics/Rheology.h"
#include "physics/Damage.h"
#include "quantities/IMaterial.h"
#include "quantities/Storage.h"
#include "thread/Pool.h"

NAMESPACE_SPH_BEGIN

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

    // reduce stress tensor by damage
    damage->reduce(scheduler, storage, DamageFlag::PRESSURE | DamageFlag::STRESS_TENSOR, material);
    ArrayView<TracelessTensor> S, S_dmg;
    tie(S, S_dmg) = storage.modify<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);

    const Float limit = material->getParam<Float>(BodySettingsId::ELASTICITY_LIMIT);
    const Float u_melt = material->getParam<Float>(BodySettingsId::MELT_ENERGY);
    IndexSequence seq = material.sequence();
    constexpr Float eps = 1.e-15_f;
    parallelFor(scheduler, *seq.begin(), *seq.end(), [&](const Size i) INL {
        // compute yielding stress
        const Float unorm = u[i] / u_melt;
        const Float y = unorm < 1.e-5_f ? limit : limit * max(1.f - unorm, 0._f);
        ASSERT(limit > 0._f);

        // apply reduction to stress tensor
        if (y < EPS) {
            reducing[i] = 0._f;
            S[i] = TracelessTensor::null();
            return;
        }
        // compute second invariant using damaged stress tensor
        const TracelessTensor s = S_dmg[i] + TracelessTensor(eps);
        const TracelessTensor sy = s / y + TracelessTensor(eps);
        const Float inv = 0.5_f * ddot(sy, sy) + eps;
        ASSERT(isReal(inv) && inv > 0._f);
        const Float red = min(sqrt(1._f / (3._f * inv)), 1._f);
        ASSERT(red >= 0._f && red <= 1._f);
        reducing[i] = red;

        // apply yield reduction in place
        S[i] = S[i] * red;
        ASSERT(isReal(S[i]));
    });

    // now we have to compute new values of damage stress tensor
    damage->reduce(scheduler, storage, DamageFlag::STRESS_TENSOR | DamageFlag::REDUCTION_FACTOR, material);
}

void VonMisesRheology::integrate(IScheduler& scheduler, Storage& storage, const MaterialView material) {
    damage->integrate(scheduler, storage, material);
}

DruckerPragerRheology::DruckerPragerRheology(AutoPtr<IFractureModel>&& damage)
    : damage(std::move(damage)) {
    if (this->damage == nullptr) {
        this->damage = makeAuto<NullFracture>();
    }
}

/*Float plasticStrain(const Float p) {
    ASSERT(p_bd < p_bp);
    if (Y_d < Y_i) {
        // discrete brittle failure along microcracks
        return lerp(0.01_f, 0.05_f, p / p_bd);
    } else if (p > 2._f * Y) {
        ASSERT(Y >= p_bp && Y <= 2._f * p_bp);
        // motion of point defects, dislocations, and twins or from grain boundary sliding
        return lerp(0.1_f, 1._f, (p - p_bp) / p_bp);
    } else {
        // both brittle fracture and ductile flow
        return lerp(0.05_f, 0.1_f, (p - p_bd) / (p_bp - b_bd));
    }
}*/

DruckerPragerRheology::~DruckerPragerRheology() = default;

void DruckerPragerRheology::create(Storage& storage,
    IMaterial& material,
    const MaterialInitialContext& context) const {
    ASSERT(storage.getMaterialCnt() == 1);
    /// \todo implement, this is copy+paste of von mises
    storage.insert<Float>(QuantityId::STRESS_REDUCING, OrderEnum::ZERO, 1._f);
    damage->setFlaws(storage, material, context);
}

void DruckerPragerRheology::initialize(IScheduler& scheduler, Storage& storage, const MaterialView material) {
    /// ArrayView<Float> u = storage.getValue<Float>(QuantityId::ENERGY); \todo dependence on melt energy
    ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    ArrayView<Float> D = storage.getValue<Float>(QuantityId::DAMAGE);

    yieldingStress.resize(storage.getParticleCnt());

    const IndexSequence seq = material.sequence();
    parallelFor(scheduler, *seq.begin(), *seq.end(), [this, material, p, D](const Size i) {
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
            yieldingStress[i] = Y_i;
        } else {
            const Float d = pow<3>(D[i]);
            const Float Y = (1._f - d) * Y_i + d * Y_d;
            yieldingStress[i] = Y;
        }
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
