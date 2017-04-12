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
    MaterialSequence material = storage.getMaterial(0);
    storage.insert<Float>(
        QuantityIds::DAMAGE, OrderEnum::FIRST, settings.get<Float>(BodySettingsIds::DAMAGE));
    material->range(QuantityIds::DAMAGE) = settings.get<Range>(BodySettingsIds::DAMAGE_RANGE);
    material->minimal(QuantityIds::DAMAGE) = settings.get<Float>(BodySettingsIds::DAMAGE_MIN);

    storage.insert<Float>(QuantityIds::EPS_MIN, OrderEnum::ZERO, 0._f);
    storage.insert<Float>(QuantityIds::M_ZERO, OrderEnum::ZERO, 0._f);
    storage.insert<Float>(QuantityIds::EXPLICIT_GROWTH, OrderEnum::ZERO, 0._f);
    storage.insert<Size>(QuantityIds::N_FLAWS, OrderEnum::ZERO, 0);
    ArrayView<Float> rho, m, eps_min, m_zero, growth;
    tie(rho, m, eps_min, m_zero, growth) = storage.getValues<Float>(QuantityIds::DENSITY,
        QuantityIds::MASSES,
        QuantityIds::EPS_MIN,
        QuantityIds::M_ZERO,
        QuantityIds::EXPLICIT_GROWTH);
    ArrayView<Size> n_flaws = storage.getValue<Size>(QuantityIds::N_FLAWS);
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityIds::POSITIONS);
    ArrayView<Size> activationIdx;
    if (options == ExplicitFlaws::ASSIGNED) {
        activationIdx = storage.getValue<Size>(QuantityIds::FLAW_ACTIVATION_IDX);
    }
    const Float mu = settings.get<Float>(BodySettingsIds::SHEAR_MODULUS);
    const Float A = settings.get<Float>(BodySettingsIds::BULK_MODULUS);
    // here all particles have the same material
    /// \todo needs to be generalized for setting up initial conditions with heterogeneous material.
    const Float youngModulus = mu * 9._f * A / (3._f * A + mu);
    material->setParams(BodySettingsIds::YOUNG_MODULUS, youngModulus);

    const Float cgFactor = settings.get<Float>(BodySettingsIds::RAYLEIGH_SOUND_SPEED);
    const Float rho0 = settings.get<Float>(BodySettingsIds::DENSITY);
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
    const Float k_weibull = settings.get<Float>(BodySettingsIds::WEIBULL_COEFFICIENT);
    const Float m_weibull = settings.get<Float>(BodySettingsIds::WEIBULL_EXPONENT);
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

void ScalarDamage::reduce(Storage& storage, const MaterialSequence sequence) {
    ArrayView<Float> damage = storage.getValue<Float>(QuantityIds::DAMAGE);
    // we can reduce pressure in place as the original value can be computed from equation of state
    ArrayView<Float> p = storage.getValue<Float>(QuantityIds::PRESSURE);
    // stress tensor is evolved in time, so we need to keep unchanged value; create modified value
    ArrayView<TracelessTensor> s = storage.getModification<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS);

    for (Size i : sequence) {
        const Float d = pow<3>(damage[i]);
        // pressure is reduced only for negative values
        /// \todo could be vectorized, maybe
        if (p[i] < 0._f) {
            p[i] = (1._f - d) * p[i];
        }
        // stress is reduced for both positive and negative values
        s[i] = (1._f - d) * s[i];
    }
}

void ScalarDamage::integrate(Storage& storage, const MaterialSequence material) {
    ArrayView<TracelessTensor> s, ds;
    s = storage.getPhysicalValue<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS);
    ds = storage.getDt<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS);
    ArrayView<Float> p, eps_min, m_zero, growth;
    tie(eps_min, m_zero, growth) =
        storage.getValues<Float>(QuantityIds::EPS_MIN, QuantityIds::M_ZERO, QuantityIds::EXPLICIT_GROWTH);
    p = storage.getPhysicalValue<Float>(QuantityIds::PRESSURE);
    ArrayView<Size> n_flaws = storage.getValue<Size>(QuantityIds::N_FLAWS);
    ArrayView<Float> damage, ddamage;
    tie(damage, ddamage) = storage.getAll<Float>(QuantityIds::DAMAGE);
    for (Size i : material) {
        // if damage is already on max value, set stress to zero to avoid limiting timestep by non-existent
        // stresses
        const Range range = material->range(QuantityIds::DAMAGE);
        if (damage[i] == range.upper()) {
            // s[i] = TracelessTensor::null();
            ds[i] = TracelessTensor::null();
            // continue;
        }
        const Tensor sigma = s[i] - p[i] * Tensor::identity();
        Float sig1, sig2, sig3;
        tie(sig1, sig2, sig3) = findEigenvalues(sigma);
        const Float sigMax = max(sig1, sig2, sig3);
        const Float young = material->getParam<Float>(BodySettingsIds::YOUNG_MODULUS);
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
}

void TensorDamage::setFlaws(Storage& UNUSED(storage), const BodySettings& UNUSED(settings)) const {
    NOT_IMPLEMENTED;
}

void TensorDamage::reduce(Storage& UNUSED(storage), const MaterialSequence UNUSED(material)) {
    NOT_IMPLEMENTED;
}

void TensorDamage::integrate(Storage& UNUSED(storage), const MaterialSequence UNUSED(material)) {
    NOT_IMPLEMENTED;
}

void NullDamage::setFlaws(Storage& UNUSED(storage), const BodySettings& UNUSED(settings)) const {}

void NullDamage::reduce(Storage& UNUSED(storage), const MaterialSequence UNUSED(material)) {}

void NullDamage::integrate(Storage& UNUSED(storage), const MaterialSequence UNUSED(material)) {}

NAMESPACE_SPH_END
