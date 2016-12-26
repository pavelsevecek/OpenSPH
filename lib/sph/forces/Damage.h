#pragma once

#include "geometry/Vector.h"
#include "math/rng/Rng.h"
#include "objects/containers/ArrayView.h"
#include "storage/QuantityMap.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

class DummyDamage : public Object {
public:
    DummyDamage(const GlobalSettings& UNUSED(settings)) {}
    INLINE Float reduce(const Float p, const int UNUSED(i)) const { return p; }

    INLINE TracelessTensor reduce(const TracelessTensor& s, const int UNUSED(i)) const { return s; }
};


/// Scalar damage describing fragmentation of the body according to Grady-Kipp model (Grady and Kipp, 1980)
class ScalarDamage : public Object {
private:
    // here d actually contains third root of damage
    ArrayView<Float> damage;
    Float kernelRadius;

public:
    ScalarDamage(const GlobalSettings& settings) { kernelRadius = Factory::getKernel<3>(settings).radius(); }

    void initialize(Storage& storage, const BodySettings& settings) const {
        storage.emplace<Float, OrderEnum::FIRST_ORDER>(QuantityKey::D,
            settings.get<Float>(BodySettingsIds::DAMAGE),
            settings.get<Range>(BodySettingsIds::DAMAGE_RANGE));
        storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::EPS, 0._f);
        storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::M_ZERO, 0._f);
        storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::DD_DT, 0._f);
        storage.emplace<int, OrderEnum::ZERO_ORDER>(QuantityKey::N_FLAWS, 0);
        ArrayView<Float> rho, m, eps_min, m_zero, growth;
        tieToArray(rho, m, eps_min, m_zero, growth) = storage.getValues<Float>(
            QuantityKey::RHO, QuantityKey::M, QuantityKey::EPS, QuantityKey::M_ZERO, QuantityKey::DD_DT);
        ArrayView<int> n_flaws = storage.getValue<int>(QuantityKey::N_FLAWS);
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityKey::R);
        const Float mu = settings.get<Float>(BodySettingsIds::SHEAR_MODULUS);
        const Float A = settings.get<Float>(BodySettingsIds::BULK_MODULUS);
        // here all particles have the same material
        /// \todo needs to be generalized for setting up initial conditions with heterogeneous material.
        storage.getMaterial(0).youngModulus = mu * 9._f * A / (3._f * A + mu);

        const Float k_weibull = settings.get<Float>(BodySettingsIds::WEIBULL_COEFFICIENT);
        const Float m_weibull = settings.get<Float>(BodySettingsIds::WEIBULL_EXPONENT);
        const Float cgFactor = settings.get<Float>(BodySettingsIds::RAYLEIGH_SOUND_SPEED);
        const Float rho0 = settings.get<Float>(BodySettingsIds::DENSITY);
        const Float cg = cgFactor * Math::sqrt((A + 4._f / 3._f * mu) / rho0);

        const int size = storage.getParticleCnt();
        // compute explicit growth
        for (int i = 0; i < size; ++i) {
            growth[i] = cg / (kernelRadius * r[i][H]);
        }
        // find volume used to normalize fracture model
        Float V = 0._f;
        for (int i = 0; i < size; ++i) {
            V += m[i] / rho[i];
        }
        Array<Float> eps_max(size);
        const Float denom = 1._f / Math::pow(k_weibull * V, 1._f / m_weibull);
        BenzAsphaugRng rng(1234);
        int flawedCnt = 0, p = 0;
        while (flawedCnt < size) {
            const int i = int(rng() * size);
            const Float eps = denom * Math::pow(Float(p), 1._f / m_weibull);
            if (n_flaws[i] == 0) {
                flawedCnt++;
                eps_min[i] = eps;
            }
            eps_max[i] = eps;
            p++;
            n_flaws[i]++;
        }
        for (int i = 0; i < size; ++i) {
            if (n_flaws[i] == 1) {
                m_zero[i] = 1._f;
            } else {
                const Float ratio = eps_max[i] / eps_min[i];
                ASSERT(Math::isReal(ratio));
                m_zero[i] = log(n_flaws[i]) / log(ratio);
            }
        }
    }

    void update(Storage& storage) { damage = storage.getValue<Float>(QuantityKey::D); }

    void integrate(Storage& storage) {
        ArrayView<TracelessTensor> s = storage.getValue<TracelessTensor>(QuantityKey::S);
        ArrayView<Float> p, eps_min, m_zero, n_flaws, growth;
        tieToArray(p, eps_min, m_zero, n_flaws, growth) = storage.getValues<Float>(
            QuantityKey::P, QuantityKey::EPS, QuantityKey::M_ZERO, QuantityKey::N_FLAWS, QuantityKey::DD_DT);
        ArrayView<Float> ddamage = storage.getAll<Float>(QuantityKey::D)[1];
        for (int i = 0; i < p.size(); ++i) {
            /// \todo apply reduction from yielding!!
            Tensor sigma = reduce(s[i], i) - reduce(p[i], i) * Tensor::identity();
            float sig1, sig2, sig3;
            tieToStatic(sig1, sig2, sig3) = findEigenvalues(sigma);
            float sigMax = Math::max(sig1, sig2, sig3);
            const Float young = reduce(storage.getMaterial(i).youngModulus, i);
            const Float strain = sigMax / young;
            const Float ratio = strain / eps_min[i];
            if (ratio <= 1._f) {
                continue;
            }
            ddamage[i] = growth[i] * Math::root<3>(Math::min(Math::pow(ratio, m_zero[i]), n_flaws[i]));
        }
    }


    /// Reduce pressure
    INLINE Float reduce(const Float p, const int i) const {
        const Float d = Math::pow<3>(damage[i]);
        if (p < 0._f) {
            return (1._f - d) * p;
        } else {
            return p;
        }
    }

    /// Reduce deviatoric stress tensor
    INLINE TracelessTensor reduce(const TracelessTensor& s, const int i) const {
        const Float d = Math::pow<3>(damage[i]);
        return (1._f - d) * s;
    }
};

NAMESPACE_SPH_END
