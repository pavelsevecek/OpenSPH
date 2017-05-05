#pragma once

#include "sph/equations/EquationTerm.h"

NAMESPACE_SPH_BEGIN


class EnergyLaplacian : public Abstract::Derivative {
private:
    ArrayView<Float> du;
    ArrayView<const Float> u, m, rho;
    ArrayView<const Vector> r;
    ArrayView<const Float> alpha;

public:
    virtual void create(Accumulated& results) override {
        results.insert<Float>(QuantityId::ENERGY);
    }

    virtual void initialize(const Storage& input, Accumulated& results) override {
        tie(u, m, rho, alpha) = input.getValues<Float>(
            QuantityId::ENERGY, QuantityId::MASSES, QuantityId::DENSITY /*, QuantityId::DIFFUSIVITY*/);
        r = input.getValue<Vector>(QuantityId::POSITIONS);
        du = results.getValue<Float>(QuantityId::ENERGY);
    }

    virtual void compute(const Size i, ArrayView<const Size> neighs, ArrayView<const Vector> grads) override {
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k];
            const Float laplacian = 2._f * dot(r[j] - r[i], grads[k]) / getSqrLength(r[j] - r[i]);
            const Float f = (u[j] - u[i]) * laplacian;
            du[i] -= m[j] / rho[j] * f;
            du[j] += m[i] / rho[i] * f;
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