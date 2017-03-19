#include "physics/Yielding.h"
#include "objects/wrappers/Finally.h"
#include "quantities/Material.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

void VonMises::initialize(Storage& storage, const BodySettings& settings) const {
    MaterialAccessor(storage).setParams(BodySettingsIds::ELASTICITY_LIMIT, settings);
    MaterialAccessor(storage).setParams(BodySettingsIds::MELT_ENERGY, settings);
    storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::YIELDING_REDUCE, 1._f);
}

void VonMises::update(Storage& storage) {
    MaterialAccessor material(storage);
    ArrayView<Float> u = storage.getValue<Float>(QuantityIds::ENERGY);
    ArrayView<Float> reducing = storage.getValue<Float>(QuantityIds::YIELDING_REDUCE);
    ArrayView<TracelessTensor> S = storage.getValue<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS);
    ArrayView<Float> D;
    if (storage.has(QuantityIds::DAMAGE)) {
        D = storage.getValue<Float>(QuantityIds::DAMAGE);
    }

    for (Size i = 0; i < storage.getParticleCnt(); ++i) {
        // compute yielding stress
        const Float limit = material.getParam<Float>(BodySettingsIds::ELASTICITY_LIMIT, i);
        const Float u0 = material.getParam<Float>(BodySettingsIds::MELT_ENERGY, i);
        const Float unorm = u[i] / u0;
        const Float y = unorm < 1.e-5 ? limit : limit * max(1.f - unorm, 0._f);
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

void DruckerPrager::update(Storage& storage) {
    yieldingStress.clear();
    /// ArrayView<Float> u = storage.getValue<Float>(QuantityIds::ENERGY); \todo dependence on melt energy
    ArrayView<Float> p = storage.getValue<Float>(QuantityIds::PRESSURE);
    ArrayView<Float> D = storage.getValue<Float>(QuantityIds::DAMAGE);
    MaterialAccessor material(storage);
    for (Size i = 0; i < storage.getParticleCnt(); ++i) {
        const Float Y_0 = material.getParam<Float>(BodySettingsIds::COHESION, i);
        const Float mu_i = material.getParam<Float>(BodySettingsIds::INTERNAL_FRICTION, i);
        const Float Y_M = material.getParam<Float>(BodySettingsIds::ELASTICITY_LIMIT, i);
        // const Float u_melt = storage.getParam<Float>(BodySettingsIds::MELT_ENERGY);
        const Float Y_i = Y_0 + mu_i * p[i] / (1._f + mu_i * p[i] / (Y_M - Y_0));
        ASSERT(Y_i >= 0);

        const Float mu_d = material.getParam<Float>(BodySettingsIds::DRY_FRICTION, i);
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


NAMESPACE_SPH_END
