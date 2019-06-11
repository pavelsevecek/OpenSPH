#include "physics/Damage.h"
#include "io/Logger.h"
#include "math/rng/Rng.h"
#include "quantities/IMaterial.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

//-----------------------------------------------------------------------------------------------------------
// ScalarGradyKippModel implementation
//-----------------------------------------------------------------------------------------------------------

void ScalarGradyKippModel::setFlaws(Storage& storage,
    IMaterial& material,
    const MaterialInitialContext& context) const {
    VERBOSE_LOG

    ASSERT(storage.getMaterialCnt() == 1);
    storage.insert<Float>(
        QuantityId::DAMAGE, OrderEnum::FIRST, material.getParam<Float>(BodySettingsId::DAMAGE));
    material.setRange(QuantityId::DAMAGE, BodySettingsId::DAMAGE_RANGE, BodySettingsId::DAMAGE_MIN);

    ASSERT(!storage.has(QuantityId::EPS_MIN) && !storage.has(QuantityId::M_ZERO) &&
               !storage.has(QuantityId::EXPLICIT_GROWTH) && !storage.has(QuantityId::N_FLAWS),
        "Recreating flaws");
    storage.insert<Float>(QuantityId::EPS_MIN, OrderEnum::ZERO, 0._f);
    storage.insert<Float>(QuantityId::M_ZERO, OrderEnum::ZERO, 0._f);
    storage.insert<Float>(QuantityId::EXPLICIT_GROWTH, OrderEnum::ZERO, 0._f);
    storage.insert<Size>(QuantityId::N_FLAWS, OrderEnum::ZERO, 0);
    ArrayView<Float> rho, m, eps_min, m_zero, growth;
    tie(rho, m, eps_min, m_zero, growth) = storage.getValues<Float>(QuantityId::DENSITY,
        QuantityId::MASS,
        QuantityId::EPS_MIN,
        QuantityId::M_ZERO,
        QuantityId::EXPLICIT_GROWTH);
    ArrayView<Size> n_flaws = storage.getValue<Size>(QuantityId::N_FLAWS);
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);

    const Float mu = material.getParam<Float>(BodySettingsId::SHEAR_MODULUS);
    const Float A = material.getParam<Float>(BodySettingsId::BULK_MODULUS);
    // here all particles have the same material
    /// \todo needs to be generalized for setting up initial conditions with heterogeneous material.
    const Float youngModulus = mu * 9._f * A / (3._f * A + mu);
    material.setParam(BodySettingsId::YOUNG_MODULUS, youngModulus);

    const Float cgFactor = material.getParam<Float>(BodySettingsId::RAYLEIGH_SOUND_SPEED);
    const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
    const Float cg = cgFactor * sqrt((A + 4._f / 3._f * mu) / rho0);

    const Size size = storage.getParticleCnt();
    // compute explicit growth
    for (Size i = 0; i < size; ++i) {
        growth[i] = cg / (context.kernelRadius * r[i][H]);
    }
    // find volume used to normalize fracture model
    Float V = 0._f;
    for (Size i = 0; i < size; ++i) {
        V += m[i] / rho[i];
    }
    ASSERT(V > 0.f);
    const Float k_weibull = material.getParam<Float>(BodySettingsId::WEIBULL_COEFFICIENT);
    const Float m_weibull = material.getParam<Float>(BodySettingsId::WEIBULL_EXPONENT);
    const bool sampleDistribution = material.getParam<bool>(BodySettingsId::WEIBULL_SAMPLE_DISTRIBUTIONS);

    // cannot use pow on k_weibull*V, leads to float overflow for larger V
    const Float denom = 1._f / (std::pow(k_weibull, 1._f / m_weibull) * std::pow(V, 1.f / m_weibull));
    ASSERT(isReal(denom) && denom > 0.f);
    Array<Float> eps_max(size);

    if (sampleDistribution) {
        // estimate of the highest iteration
        const Float p_max = size * log(size);
        const Float mult = exp(p_max / size) - 1._f;
        for (Size i = 0; i < size; ++i) {
            const Float x = context.rng();

            // sample with exponential distribution
            const Float p1 = -Float(size) * log(1._f - x);
            const Float p2 = Float(size) * log(1._f + x * mult);

            eps_min[i] = denom * std::pow(p1, 1._f / m_weibull);
            eps_max[i] = denom * std::pow(max(p1, p2), 1._f / m_weibull);
            ASSERT(eps_min[i] > 0 && eps_min[i] <= eps_max[i], eps_min[i], eps_max[i]);

            // sample with Poisson distribution
            const Float mu = log(size);
            n_flaws[i] = max<int>(1, samplePoissonDistribution(*context.rng, mu));

            // ensure that m_zero >= 1
            // n_flaws[i] = max(n_flaws[i], Size(ceil(eps_max[i] / eps_min[i])));
            eps_max[i] = min(eps_max[i], n_flaws[i] * eps_min[i]);
            ASSERT(n_flaws[i] >= eps_max[i] / eps_min[i]);
        }
    } else {
        Size flawedCnt = 0, p = 1;
        while (flawedCnt < size) {
            const Size i = Size(context.rng() * size);
            const Float eps = denom * std::pow(Float(p), 1._f / m_weibull);
            ASSERT(isReal(eps) && eps > 0.f);
            if (n_flaws[i] == 0) {
                flawedCnt++;
                eps_min[i] = eps;
            }
            eps_max[i] = eps;
            ASSERT(eps_max[i] >= eps_min[i]);
            p++;
            n_flaws[i]++;
        }
    }
    for (Size i = 0; i < size; ++i) {
        if (n_flaws[i] == 1) {
            // special case to avoid division by zero below
            m_zero[i] = 1._f;
        } else {
            const Float ratio = eps_max[i] / eps_min[i];
            ASSERT(ratio >= 1._f, eps_min[i], eps_max[i]);

            m_zero[i] = log(n_flaws[i]) / log(ratio);
            ASSERT(m_zero[i] >= 1._f, m_zero[i], n_flaws[i], eps_min[i], eps_max[i]);
        }
    }
}

void ScalarGradyKippModel::integrate(IScheduler& scheduler, Storage& storage, const MaterialView material) {
    VERBOSE_LOG

    ArrayView<TracelessTensor> s, ds;
    s = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ds = storage.getDt<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<Float> p, eps_min, m_zero, growth;
    tie(eps_min, m_zero, growth) =
        storage.getValues<Float>(QuantityId::EPS_MIN, QuantityId::M_ZERO, QuantityId::EXPLICIT_GROWTH);
    p = storage.getValue<Float>(QuantityId::PRESSURE);
    ArrayView<Size> n_flaws = storage.getValue<Size>(QuantityId::N_FLAWS);
    ArrayView<Float> damage, ddamage;
    tie(damage, ddamage) = storage.getAll<Float>(QuantityId::DAMAGE);

    IndexSequence seq = material.sequence();
    parallelFor(scheduler, *seq.begin(), *seq.end(), [&](const Size i) {
        const Interval range = material->range(QuantityId::DAMAGE);

        if (damage[i] >= range.upper()) {
            // We CANNOT set derivative of damage to zero, it would break predictor-corrector integrator!
            // Instead, we set damage derivative to large value, so that it is larger than the derivative from
            // prediction, therefore damage will INCREASE in corrections, but will be immediately clamped
            // to 1 TOGETHER WITH DERIVATIVES, time step is computed afterwards, so it should be ok.
            ddamage[i] = LARGE;
            return;
        }
        const SymmetricTensor sigma = SymmetricTensor(s[i]) - p[i] * SymmetricTensor::identity();
        Float sig1, sig2, sig3;
        tie(sig1, sig2, sig3) = findEigenvalues(sigma);
        const Float sigMax = max(sig1, sig2, sig3);
        const Float young = material->getParam<Float>(BodySettingsId::YOUNG_MODULUS);
        // we need to assume reduces Young modulus here, hence 1-D factor
        const Float young_red = max((1._f - pow<3>(damage[i])) * young, 1.e-20_f);
        const Float strain = sigMax / young_red;
        const Float ratio = strain / eps_min[i];
        ASSERT(isReal(ratio));
        if (ratio <= 1._f) {
            return;
        }
        ddamage[i] = growth[i] * root<3>(min(std::pow(ratio, m_zero[i]), Float(n_flaws[i])));
        ASSERT(ddamage[i] >= 0.f);
    });
}

//-----------------------------------------------------------------------------------------------------------
// TensorGradyKippModel implementation
//-----------------------------------------------------------------------------------------------------------

void TensorGradyKippModel::setFlaws(Storage& UNUSED(storage),
    IMaterial& UNUSED(material),
    const MaterialInitialContext& UNUSED(context)) const {
    NOT_IMPLEMENTED;
}

/*void TensorGradyKippModel::reduce(IScheduler& scheduler,
    Storage& storage,
    const Flags<DamageFlag> flags,
    const MaterialView material) {
    ArrayView<SymmetricTensor> damage = storage.getValue<SymmetricTensor>(QuantityId::DAMAGE);
    // we can reduce pressure in place as the original value can be computed from equation of state
    ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    ArrayView<Float> reduce = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
    // stress tensor is evolved in time, so we need to keep unchanged value; create modified value
    ArrayView<TracelessTensor> s, s_dmg;
    tie(s, s_dmg) = storage.modify<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);

    IndexSequence seq = material.sequence();
    parallelFor(scheduler, *seq.begin(), *seq.end(), [&](const Size i) INL {
        // const Float d = pow<3>(damage[i]);
        const SymmetricTensor d = damage[i]; /// \todo should we have third root here?

        const SymmetricTensor f = SymmetricTensor::identity() - d;

        SymmetricTensor sigmaD;
        if (p[i] < 0._f) {
            const SymmetricTensor sigma = SymmetricTensor(s[i]) - p[i] * SymmetricTensor::identity();
            sigmaD = f * sigma * f;
        } else {
            sigmaD = f * SymmetricTensor(s[i]) * f - p[i] * SymmetricTensor::identity();
        }
        const Float pD = -sigmaD.trace() / 3._f;
        const TracelessTensor sD = TracelessTensor(sigmaD + pD * SymmetricTensor::identity());

        // pressure is reduced only for negative values
        if (flags.has(DamageFlag::PRESSURE)) {
            p[i] = pD;
        }
        // stress is reduced for both positive and negative values
        if (flags.has(DamageFlag::STRESS_TENSOR)) {
            s_dmg[i] = sD;
        }
        if (flags.has(DamageFlag::REDUCTION_FACTOR)) {
            reduce[i] = (1._f - d.trace()) * reduce[i];
        }
    });
}*/

void TensorGradyKippModel::integrate(IScheduler& UNUSED(scheduler),
    Storage& UNUSED(storage),
    const MaterialView UNUSED(material)) {
    NOT_IMPLEMENTED;
}

void NullFracture::setFlaws(Storage& UNUSED(storage),
    IMaterial& UNUSED(material),
    const MaterialInitialContext& UNUSED(context)) const {}

void NullFracture::integrate(IScheduler& UNUSED(scheduler),
    Storage& UNUSED(storage),
    const MaterialView UNUSED(material)) {}

NAMESPACE_SPH_END
