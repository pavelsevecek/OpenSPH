#pragma once

#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN


class EnergyLaplacian : public DerivativeTemplate<EnergyLaplacian> {
private:
    ArrayView<Float> du;
    ArrayView<const Float> u, m, rho;
    ArrayView<const Vector> r;
    ArrayView<const Float> alpha;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Float>(QuantityId::ENERGY, OrderEnum::FIRST);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(u, m, rho, alpha) = input.getValues<Float>(
            QuantityId::ENERGY, QuantityId::MASSES, QuantityId::DENSITY /*, QuantityId::DIFFUSIVITY*/);
        r = input.getValue<Vector>(QuantityId::POSITIONS);
        du = results.getBuffer<Float>(QuantityId::ENERGY, OrderEnum::FIRST);
    }

    template <bool Symmetric>
    INLINE void eval(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) {
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            const Float f = laplacian(u[j] - u[i], r[j] - r[i], grads[k]);
            du[i] -= m[j] / rho[j] * f;
            if (Symmetric) {
                du[j] += m[i] / rho[i] * f;
            }
        }
    }
};

class HeatDiffusionEquation : public Abstract::EquationTerm {
public:
    virtual void setDerivatives(DerivativeHolder& derivatives, const RunSettings& settings) override {
        // add laplacian of energy to the list of derivatives
        derivatives.require<EnergyLaplacian>(settings);
    }

    virtual void initialize(Storage& UNUSED(storage)) override {}

    virtual void finalize(Storage& UNUSED(storage)) override {}

    virtual void create(Storage& UNUSED(storage), Abstract::Material& UNUSED(material)) const override {}
};


NAMESPACE_SPH_END
