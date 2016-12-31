#include "sph/forces/Damage.h"
#include "math/rng/Rng.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN


ScalarDamage::ScalarDamage(const GlobalSettings& settings,
    const Yielding& yielding,
    const ExplicitFlaws options)
    : yielding(yielding)
    , options(options) {
    kernelRadius = Factory::getKernel<3>(settings).radius();
}

void ScalarDamage::initialize(Storage& storage, const BodySettings& settings) const {
    storage.emplace<Float, OrderEnum::FIRST_ORDER>(QuantityKey::DAMAGE,
        settings.get<Float>(BodySettingsIds::DAMAGE),
        settings.get<Range>(BodySettingsIds::DAMAGE_RANGE));
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::EPS_MIN, 0._f);
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::M_ZERO, 0._f);
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::EXPLICIT_GROWTH, 0._f);
    storage.emplace<Size, OrderEnum::ZERO_ORDER>(QuantityKey::N_FLAWS, 0);
    ArrayView<Float> rho, m, eps_min, m_zero, growth;
    tie(rho, m, eps_min, m_zero, growth) = storage.getValues<Float>(QuantityKey::DENSITY,
        QuantityKey::MASSES,
        QuantityKey::EPS_MIN,
        QuantityKey::M_ZERO,
        QuantityKey::EXPLICIT_GROWTH);
    ArrayView<Size> n_flaws = storage.getValue<Size>(QuantityKey::N_FLAWS);
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityKey::POSITIONS);
    ArrayView<Size> activationIdx;
    if (options == ExplicitFlaws::ASSIGNED) {
        activationIdx = storage.getValue<Size>(QuantityKey::FLAW_ACTIVATION_IDX);
    }
    const Float mu = settings.get<Float>(BodySettingsIds::SHEAR_MODULUS);
    const Float A = settings.get<Float>(BodySettingsIds::BULK_MODULUS);
    // here all particles have the same material
    /// \todo needs to be generalized for setting up initial conditions with heterogeneous material.
    storage.getMaterial(0).youngModulus = mu * 9._f * A / (3._f * A + mu);

    const Float cgFactor = settings.get<Float>(BodySettingsIds::RAYLEIGH_SOUND_SPEED);
    const Float rho0 = settings.get<Float>(BodySettingsIds::DENSITY);
    const Float cg = cgFactor * Math::sqrt((A + 4._f / 3._f * mu) / rho0);

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
    const Float k_weibull = settings.get<Float>(BodySettingsIds::WEIBULL_COEFFICIENT);
    const Float m_weibull = settings.get<Float>(BodySettingsIds::WEIBULL_EXPONENT);
    const Float denom = 1._f / Math::pow(k_weibull * V, 1._f / m_weibull);
    Array<Float> eps_max(size);
    BenzAsphaugRng rng(1234); /// \todo generalize random number generator
    Size flawedCnt = 0, p = 1;
    while (flawedCnt < size) {
        const Size i = Size(rng() * size);
        if (options == ExplicitFlaws::ASSIGNED) {
            p = activationIdx[i];
        }
        const Float eps = denom * Math::pow(Float(p), 1._f / m_weibull);
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
            ASSERT(Math::isReal(ratio));
            m_zero[i] = log(n_flaws[i]) / log(ratio);
        }
    }
}

void ScalarDamage::integrate(Storage& storage) {
    ArrayView<TracelessTensor> s = storage.getValue<TracelessTensor>(QuantityKey::DEVIATORIC_STRESS);
    ArrayView<Float> p, eps_min, m_zero, growth;
    tie(p, eps_min, m_zero, growth) = storage.getValues<Float>(
        QuantityKey::PRESSURE, QuantityKey::EPS_MIN, QuantityKey::M_ZERO, QuantityKey::EXPLICIT_GROWTH);
    ArrayView<Size> n_flaws = storage.getValue<Size>(QuantityKey::N_FLAWS);
    ArrayView<Float> ddamage = storage.getAll<Float>(QuantityKey::DAMAGE)[1];
    for (Size i = 0; i < p.size(); ++i) {
        Tensor sigma = yielding(reduce(s[i], i), i) - reduce(p[i], i) * Tensor::identity();
        float sig1, sig2, sig3;
        tie(sig1, sig2, sig3) = findEigenvalues(sigma);
        float sigMax = Math::max(sig1, sig2, sig3);
        const Float young = reduce(storage.getMaterial(i).youngModulus, i);
        const Float strain = sigMax / young;
        const Float ratio = strain / eps_min[i];
        if (ratio <= 1._f) {
            continue;
        }
        ddamage[i] = growth[i] * Math::root<3>(Math::min(Math::pow(ratio, m_zero[i]), Float(n_flaws[i])));
    }
}

NAMESPACE_SPH_END
