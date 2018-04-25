#include "physics/Damage.h"
#include "io/Logger.h"
#include "math/rng/Rng.h"
#include "quantities/IMaterial.h"
#include "quantities/Storage.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "thread/Pool.h"

NAMESPACE_SPH_BEGIN

//-----------------------------------------------------------------------------------------------------------
// ScalarGradyKippModel implementation
//-----------------------------------------------------------------------------------------------------------

ScalarGradyKippModel::ScalarGradyKippModel(const Float kernelRadius, const ExplicitFlaws options)
    : kernelRadius(kernelRadius)
    , options(options) {}

ScalarGradyKippModel::ScalarGradyKippModel(const RunSettings& settings, const ExplicitFlaws options)
    : ScalarGradyKippModel(Factory::getKernel<3>(settings).radius(), options) {}

void ScalarGradyKippModel::setFlaws(Storage& storage,
    IMaterial& material,
    const MaterialInitialContext& context) const {
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
    ArrayView<Size> activationIdx;
    if (options == ExplicitFlaws::ASSIGNED) {
        activationIdx = storage.getValue<Size>(QuantityId::FLAW_ACTIVATION_IDX);
    }
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
        growth[i] = cg / (kernelRadius * r[i][H]);
    }
    // find volume used to normalize fracture model
    Float V = 0._f;
    for (Size i = 0; i < size; ++i) {
        V += m[i] / rho[i];
    }
    ASSERT(V > 0.f);
    const Float k_weibull = material.getParam<Float>(BodySettingsId::WEIBULL_COEFFICIENT);
    const Float m_weibull = material.getParam<Float>(BodySettingsId::WEIBULL_EXPONENT);
    // cannot use pow on k_weibull*V, leads to float overflow for larger V
    const Float denom = 1._f / (std::pow(k_weibull, 1._f / m_weibull) * std::pow(V, 1.f / m_weibull));
    ASSERT(isReal(denom) && denom > 0.f);
    Array<Float> eps_max(size);
    Size flawedCnt = 0, p = 1;
    while (flawedCnt < size) {
        const Size i = Size(context.rng() * size);
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

void ScalarGradyKippModel::reduce(Storage& storage,
    const Flags<DamageFlag> flags,
    const MaterialView material) {
    ArrayView<Float> damage = storage.getValue<Float>(QuantityId::DAMAGE);
    // we can reduce pressure in place as the original value can be computed from equation of state
    ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    ArrayView<Float> reduce = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
    // stress tensor is evolved in time, so we need to keep unchanged value; create modified value
    ArrayView<TracelessTensor> s, s_dmg;
    tie(s, s_dmg) = storage.modify<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);

    IndexSequence seq = material.sequence();
    parallelFor(ThreadPool::getGlobalInstance(), *seq.begin(), *seq.end(), [&](const Size i) INL {
        const Float d = pow<3>(damage[i]);
        // pressure is reduced only for negative values
        /// \todo could be vectorized, maybe
        if (flags.has(DamageFlag::PRESSURE)) {
            if (p[i] < 0._f) {
                p[i] = (1._f - d) * p[i];
            }
        }
        // stress is reduced for both positive and negative values
        if (flags.has(DamageFlag::STRESS_TENSOR)) {
            s_dmg[i] = (1._f - d) * s[i];
        }
        if (flags.has(DamageFlag::REDUCTION_FACTOR)) {
            reduce[i] = (1._f - d) * reduce[i];
        }
    });
}

void ScalarGradyKippModel::integrate(Storage& storage, const MaterialView material) {
    ArrayView<TracelessTensor> s, s_dmg, ds;
    s_dmg = storage.getPhysicalValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    s = storage.getValue<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ds = storage.getDt<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);
    ArrayView<Float> p, eps_min, m_zero, growth;
    tie(eps_min, m_zero, growth) =
        storage.getValues<Float>(QuantityId::EPS_MIN, QuantityId::M_ZERO, QuantityId::EXPLICIT_GROWTH);
    p = storage.getPhysicalValue<Float>(QuantityId::PRESSURE);
    ArrayView<Size> n_flaws = storage.getValue<Size>(QuantityId::N_FLAWS);
    ArrayView<Float> damage, ddamage;
    tie(damage, ddamage) = storage.getAll<Float>(QuantityId::DAMAGE);

    IndexSequence seq = material.sequence();
    parallelFor(ThreadPool::getGlobalInstance(), *seq.begin(), *seq.end(), [&](const Size i) {
        // if damage is already on max value, set stress to zero to avoid limiting timestep by
        // non-existent stresses
        const Interval range = material->range(QuantityId::DAMAGE);
        /// \todo skip if the stress tensor is already fully damaged?
        /// \todo can we set S derivatives to zero? This will break PC timestepping for stress tensor
        /// but all physics depend on damaged values, anyway
        if (damage[i] >= range.upper()) {
            // we CANNOT set derivative of damage to zero!
            ddamage[i] = LARGE; /// \todo is this ok?
            // we set damage derivative to large value, so that it is larger than the derivative from
            // prediction, therefore damage will INCREASE in corrections, but will be immediately clamped
            // to 1 TOGETHER WITH DERIVATIVES, time step is computed afterwards, so it should be ok.
            s[i] = TracelessTensor::null();
            ds[i] = TracelessTensor::null(); /// \todo this is the derivative used for computing time step
            return;
        }
        const SymmetricTensor sigma = SymmetricTensor(s_dmg[i]) - p[i] * SymmetricTensor::identity();
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

void TensorGradyKippModel::reduce(Storage& UNUSED(storage),
    const Flags<DamageFlag> UNUSED(flags),
    const MaterialView UNUSED(material)) {
    NOT_IMPLEMENTED;
}

void TensorGradyKippModel::integrate(Storage& UNUSED(storage), const MaterialView UNUSED(material)) {
    NOT_IMPLEMENTED;
}

void MohrCoulombModel::setFlaws(Storage& storage,
    IMaterial& UNUSED(material),
    const MaterialInitialContext& UNUSED(context)) const {
    /// \todo correct values
    storage.insert<Float>(QuantityId::MOHR_COULOMB_STRESS, OrderEnum::ZERO, 0._f);
    storage.insert<Float>(QuantityId::FRICTION_ANGLE, OrderEnum::ZERO, 0._f);
}

void MohrCoulombModel::integrate(Storage& UNUSED(storage), const MaterialView UNUSED(material)) {
    NOT_IMPLEMENTED;
    /*for (Size i = 0; i < p.size(); ++i) {
        const SymmetricTensor sigma = SymmetricTensor(s_dmg[i]) - p[i] * SymmetricTensor::identity();
        Float sig1, sig2, sig3;
        tie(sig1, sig2, sig3) = findEigenvalues(sigma);
        const Float sigMax = max(sig1, sig2, sig3);
    }*/
}

void NullFracture::setFlaws(Storage& UNUSED(storage),
    IMaterial& UNUSED(material),
    const MaterialInitialContext& UNUSED(context)) const {}

void NullFracture::reduce(Storage& storage,
    const Flags<DamageFlag> UNUSED(flags),
    const MaterialView material) {
    ArrayView<TracelessTensor> s, s_dmg;
    tie(s, s_dmg) = storage.modify<TracelessTensor>(QuantityId::DEVIATORIC_STRESS);

    IndexSequence seq = material.sequence();
    parallelFor(ThreadPool::getGlobalInstance(), *seq.begin(), *seq.end(), [&](const Size i) INL {
        s_dmg[i] = s[i];
    });
}

void NullFracture::integrate(Storage& UNUSED(storage), const MaterialView UNUSED(material)) {}

NAMESPACE_SPH_END
