#pragma once

/// \file Heat.h
/// \brief Energy transfer terms
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "quantities/IMaterial.h"
#include "sph/equations/DerivativeHelpers.h"
#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN

class EnergyLaplacian : public DerivativeTemplate<EnergyLaplacian> {
private:
    ArrayView<Float> deltaU;
    ArrayView<const Float> u, m, rho;
    ArrayView<const Vector> r;

public:
    explicit EnergyLaplacian(const RunSettings& settings)
        : DerivativeTemplate<EnergyLaplacian>(settings) {}

    INLINE void additionalCreate(Accumulated& results) {
        results.insert<Float>(QuantityId::ENERGY_LAPLACIAN, OrderEnum::ZERO, BufferSource::UNIQUE);
    }

    INLINE void additionalInitialize(const Storage& input, Accumulated& results) {
        tie(u, m, rho) = input.getValues<Float>(QuantityId::ENERGY, QuantityId::MASS, QuantityId::DENSITY);
        r = input.getValue<Vector>(QuantityId::POSITION);
        deltaU = results.getBuffer<Float>(QuantityId::ENERGY_LAPLACIAN, OrderEnum::ZERO);
    }

    INLINE bool additionalEquals(const EnergyLaplacian& UNUSED(other)) const {
        return true;
    }

    template <bool Symmetric>
    INLINE void eval(const Size i, const Size j, const Vector& grad) {
        const Float f = laplacian(u[j] - u[i], grad, r[j] - r[i]);
        deltaU[i] += m[j] / rho[j] * f;
        if (Symmetric) {
            deltaU[j] -= m[i] / rho[i] * f;
        }
    }
};

/// \todo this
class HeatDiffusionEquation : public IEquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        // add laplacian of energy to the list of derivatives
        derivatives.require(makeAuto<EnergyLaplacian>(settings));
    }

    virtual void initialize(IScheduler& UNUSED(scheduler),
        Storage& UNUSED(storage),
        const Float UNUSED(t)) override {}

    virtual void finalize(IScheduler& UNUSED(scheduler), Storage& storage, const Float UNUSED(t)) override {
        ArrayView<Float> du = storage.getDt<Float>(QuantityId::ENERGY);
        ArrayView<const Float> deltaU = storage.getValue<Float>(QuantityId::ENERGY_LAPLACIAN);
        for (Size matIdx = 0; matIdx < storage.getMaterialCnt(); ++matIdx) {
            MaterialView mat = storage.getMaterial(matIdx);
            const Float alpha = mat->getParam<Float>(BodySettingsId::DIFFUSIVITY);
            for (Size i : mat.sequence()) {
                du[i] = alpha * deltaU[i];
            }
        }
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        storage.insert<Float>(QuantityId::ENERGY_LAPLACIAN, OrderEnum::ZERO, 0._f);

        const Float u0 = material.getParam<Float>(BodySettingsId::ENERGY);
        storage.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST, u0);
        material.setRange(QuantityId::ENERGY, BodySettingsId::ENERGY_RANGE, BodySettingsId::ENERGY_MIN);
    }
};

class RadiativeCooling : public IEquationTerm {};

NAMESPACE_SPH_END
