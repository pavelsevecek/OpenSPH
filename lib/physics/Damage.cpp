#include "physics/Damage.h"
#include "math/rng/Rng.h"
#include "quantities/Material.h"
#include "quantities/Storage.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN


ScalarDamage::ScalarDamage(const GlobalSettings& settings, const ExplicitFlaws options)
    : options(options) {
    kernelRadius = Factory::getKernel<3>(settings).radius();
}

void ScalarDamage::initialize(Storage& storage, const BodySettings& settings) const {
    storage.insert<Float, OrderEnum::FIRST_ORDER>(QuantityIds::DAMAGE,
        settings.get<Float>(BodySettingsIds::DAMAGE),
        settings.get<Range>(BodySettingsIds::DAMAGE_RANGE));
    MaterialAccessor(storage).minimal(QuantityIds::DAMAGE, 0) =
        settings.get<Float>(BodySettingsIds::DAMAGE_MIN);
    storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::EPS_MIN, 0._f);
    storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::M_ZERO, 0._f);
    storage.insert<Float, OrderEnum::ZERO_ORDER>(QuantityIds::EXPLICIT_GROWTH, 0._f);
    storage.insert<Size, OrderEnum::ZERO_ORDER>(QuantityIds::N_FLAWS, 0);
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
    MaterialAccessor(storage).setParams(BodySettingsIds::YOUNG_MODULUS, youngModulus);

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
    BenzAsphaugRng rng(1234); /// \todo generalize random number generator
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

void ScalarDamage::update(Storage& storage) {
    damage = storage.getValue<Float>(QuantityIds::DAMAGE);
}

void ScalarDamage::integrate(Storage& storage) {
    ArrayView<TracelessTensor> s, ds;
    tie(s, ds) = storage.getAll<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS);
    ArrayView<Float> p, eps_min, m_zero, growth;
    tie(p, eps_min, m_zero, growth) = storage.getValues<Float>(
        QuantityIds::PRESSURE, QuantityIds::EPS_MIN, QuantityIds::M_ZERO, QuantityIds::EXPLICIT_GROWTH);
    ArrayView<Size> n_flaws = storage.getValue<Size>(QuantityIds::N_FLAWS);
    ArrayView<Float> damage, ddamage;
    tie(damage, ddamage) = storage.getAll<Float>(QuantityIds::DAMAGE);
    MaterialAccessor material(storage);
    for (Size i = 0; i < p.size(); ++i) {
        Tensor sigma = reduce(s[i], i) - reduce(p[i], i) * Tensor::identity();
        Float sig1, sig2, sig3;
        tie(sig1, sig2, sig3) = findEigenvalues(sigma);
        const Float sigMax = max(sig1, sig2, sig3);
        const Float young = material.getParam<Float>(BodySettingsIds::YOUNG_MODULUS, i);
        const Float young_red = reduce(young, i);
        const Float strain = sigMax / young_red;
        const Float ratio = strain / eps_min[i];
        if (ratio <= 1._f) {
            continue;
        }
        ddamage[i] = growth[i] * root<3>(min(std::pow(ratio, m_zero[i]), Float(n_flaws[i])));

        // if damage is already on max value, set stress to zero to avoid limiting timestep by non-existent
        // stresses
        const Range range = storage.getQuantity(QuantityIds::DAMAGE).getRange();
        if (almostEqual(damage[i], range.upper())) {
            s[i] = TracelessTensor::null();
            ds[i] = TracelessTensor::null();
        }
    }
}

NAMESPACE_SPH_END
