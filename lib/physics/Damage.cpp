#include "physics/Damage.h"
#include "io/Logger.h"
#include "math/rng/Rng.h"
#include "quantities/AbstractMaterial.h"
#include "quantities/Storage.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ScalarDamage implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

ScalarDamage::ScalarDamage(const Float kernelRadius, const ExplicitFlaws options)
    : kernelRadius(kernelRadius)
    , options(options) {}

void ScalarDamage::setFlaws(Storage& storage, const BodySettings& settings) const {
    ASSERT(storage.getMaterialCnt() == 1);
    MaterialView material = storage.getMaterial(0);
    storage.insert<Float>(QuantityId::DAMAGE, OrderEnum::FIRST, settings.get<Float>(BodySettingsId::DAMAGE));
    material->range(QuantityId::DAMAGE) = settings.get<Range>(BodySettingsId::DAMAGE_RANGE);
    material->minimal(QuantityId::DAMAGE) = settings.get<Float>(BodySettingsId::DAMAGE_MIN);

    storage.insert<Float>(QuantityId::EPS_MIN, OrderEnum::ZERO, 0._f);
    storage.insert<Float>(QuantityId::M_ZERO, OrderEnum::ZERO, 0._f);
    storage.insert<Float>(QuantityId::EXPLICIT_GROWTH, OrderEnum::ZERO, 0._f);
    storage.insert<Size>(QuantityId::N_FLAWS, OrderEnum::ZERO, 0);
    ArrayView<Float> rho, m, eps_min, m_zero, growth;
    tie(rho, m, eps_min, m_zero, growth) = storage.getValues<Float>(QuantityId::DENSITY,
        QuantityId::MASSES,
        QuantityId::EPS_MIN,
        QuantityId::M_ZERO,
        QuantityId::EXPLICIT_GROWTH);
    ArrayView<Size> n_flaws = storage.getValue<Size>(QuantityId::N_FLAWS);
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<Size> activationIdx;
    if (options == ExplicitFlaws::ASSIGNED) {
        activationIdx = storage.getValue<Size>(QuantityId::FLAW_ACTIVATION_IDX);
    }
    const Float mu = settings.get<Float>(BodySettingsId::SHEAR_MODULUS);
    const Float A = settings.get<Float>(BodySettingsId::BULK_MODULUS);
    // here all particles have the same material
    /// \todo needs to be generalized for setting up initial conditions with heterogeneous material.
    const Float youngModulus = mu * 9._f * A / (3._f * A + mu);
    material->setParam(BodySettingsId::YOUNG_MODULUS, youngModulus);

    const Float cgFactor = settings.get<Float>(BodySettingsId::RAYLEIGH_SOUND_SPEED);
    const Float rho0 = settings.get<Float>(BodySettingsId::DENSITY);
    const Float cg = cgFactor * sqrt((A + 4._f / 3._f * mu) / rho0);

    const Size size = storage.getParticleCnt();
    // compute explicit growth
    for (Size i = 0; i < size; ++i) {
        growth[i] = cg / (kernelRadius * r[i][H]);
    }
    // find volume used to normalize fracture model
    Float V = 0._f;
    for (Size i = 0; i < size; ++i) {
        V += m[i] / rho[i];
    }
    ASSERT(V > 0.f);
    const Float k_weibull = settings.get<Float>(BodySettingsId::WEIBULL_COEFFICIENT);
    const Float m_weibull = settings.get<Float>(BodySettingsId::WEIBULL_EXPONENT);
    // cannot use pow on k_weibull*V, leads to float overflow for larger V
    const Float denom = 1._f / (std::pow(k_weibull, 1._f / m_weibull) * std::pow(V, 1.f / m_weibull));
    ASSERT(isReal(denom) && denom > 0.f);
    Array<Float> eps_max(size);
    /// \todo random number generator must be shared for all bodies, but should not be global; repeated runs
    /// MUST have the same flaw distribution
    /// RNG can be stored in InitialConditions and passed to objects (in some InitialConditionContext struct),
    /// this is just a temporary fix!!!
    static BenzAsphaugRng rng(1234);
    Size flawedCnt = 0, p = 1;
    while (flawedCnt < size) {
        const Size i = Size(rng() * size);
        if (options == ExplicitFlaws::ASSIGNED) {
            p = activationIdx[i];
        }
        const Float eps = denom * std::pow(Float(p), 1._f / m_weibull);
        ASSERT(isReal(eps) && eps > 0.f);
        if (n_flaws[i] == 0) {
            flawedCnt++;
            eps_min[i] = eps;
        }
        eps_max[i] = eps;
        p++;
        n_flaws[i]++;
    }
    for (Size i = 0; i < size; ++i) {
        if (n_flaws[i] == 1) {
            m_zero[i] = 1._f;
        } else {
            const Float ratio = eps_max[i] / eps_min[i];
            ASSERT(isReal(ratio));
            m_zero[i] = log(n_flaws[i]) / log(ratio);
        }
    }
}

void ScalarDamage::reduce(Storage& storage, const MaterialView material) {
    ArrayView<Float> damage = storage.getValue<Float>(QuantityId::DAMAGE);
    // we can reduce pressure in place as the original value can be computed from equation of state
    ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    // stress tensor is evolved in time, so we need to keep unchanged value; create modified value
    ArrayView<TracelessTensor> s, s_dmg;
    tie(s, s_dmg) = storage.modify<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);

    IndexSequence seq = material.sequence();
    storage.parallelFor(*seq.begin(), *seq.end(), [&](const Size n1, const Size n2) INL {
        for (Size i = n1; i < n2; ++i) {
            const Float d = pow<3>(damage[i]);
            // pressure is reduced only for negative values
            /// \todo could be vectorized, maybe
            if (p[i] < 0._f) {
                p[i] = (1._f - d) * p[i];
            }
            // stress is reduced for both positive and negative values
            s_dmg[i] = (1._f - d) * s[i];
        }
    });
}

void ScalarDamage::integrate(Storage& storage, const MaterialView material) {
    ArrayView<TracelessTensor> s, ds;
    s = storage.getPhysicalValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ds = storage.getDt<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<Float> p, eps_min, m_zero, growth;
    tie(eps_min, m_zero, growth) =
        storage.getValues<Float>(QuantityId::EPS_MIN, QuantityId::M_ZERO, QuantityId::EXPLICIT_GROWTH);
    p = storage.getPhysicalValue<Float>(QuantityId::PRESSURE);
    ArrayView<Size> n_flaws = storage.getValue<Size>(QuantityId::N_FLAWS);
    ArrayView<Float> damage, ddamage;
    tie(damage, ddamage) = storage.getAll<Float>(QuantityId::DAMAGE);

    IndexSequence seq = material.sequence();
    storage.parallelFor(*seq.begin(), *seq.end(), [&](const Size n1, const Size n2) {
        for (Size i = n1; i < n2; ++i) {
            // if damage is already on max value, set stress to zero to avoid limiting timestep by
            // non-existent
            // stresses
            const Range range = material->range(QuantityId::DAMAGE);
            if (damage[i] == range.upper()) {
                // s[i] = TracelessTensor::null();
                ds[i] = TracelessTensor::null();
                // continue;
            }
            const Tensor sigma = s[i] - p[i] * Tensor::identity();
            Float sig1, sig2, sig3;
            tie(sig1, sig2, sig3) = findEigenvalues(sigma);
            const Float sigMax = max(sig1, sig2, sig3);
            const Float young = material->getParam<Float>(BodySettingsId::YOUNG_MODULUS);
            // we need to assume reduces Young modulus here, hence 1-D factor
            const Float young_red = max((1 - damage[i]) * young, 1.e-20_f);
            const Float strain = sigMax / young_red;
            const Float ratio = strain / eps_min[i];
            ASSERT(isReal(ratio));
            if (ratio <= 1._f) {
                continue;
            }
            ddamage[i] = growth[i] * root<3>(min(std::pow(ratio, m_zero[i]), Float(n_flaws[i])));
            ASSERT(ddamage[i] >= 0.f);
        }
    });
}

void TensorDamage::setFlaws(Storage& UNUSED(storage), const BodySettings& UNUSED(settings)) const {
    NOT_IMPLEMENTED;
}

void TensorDamage::reduce(Storage& UNUSED(storage), const MaterialView UNUSED(material)) {
    NOT_IMPLEMENTED;
}

void TensorDamage::integrate(Storage& UNUSED(storage), const MaterialView UNUSED(material)) {
    NOT_IMPLEMENTED;
}

void NullDamage::setFlaws(Storage& UNUSED(storage), const BodySettings& UNUSED(settings)) const {}

void NullDamage::reduce(Storage& UNUSED(storage), const MaterialView UNUSED(material)) {}

void NullDamage::integrate(Storage& UNUSED(storage), const MaterialView UNUSED(material)) {}

NAMESPACE_SPH_END
