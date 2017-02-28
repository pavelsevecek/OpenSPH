#pragma once

/// Solver for terms due to pressure gradient in equation of motion and energy equation

#include "geometry/Vector.h"
#include "objects/containers/ArrayView.h"
#include "objects/finders/AbstractFinder.h"
#include "system/Settings.h"
#include <map>

NAMESPACE_SPH_BEGIN


namespace Abstract {
    class Derivative {
    public:
        // sets up arrayviews
        virtual void update(Storage& input, Storage& results) = 0;

        virtual void sum(const Size idx,
            ArrayView<const NeighbourRecord> neighs,
            ArrayView<const Vector> grads) = 0;
    };
}


class DerivativeHolder {
private:
    Array<std::unique_ptr<Abstract::Derivative>> values;

public:
    template <typename TDerivative>
    void require() {
        for (auto& v : values) {
            if (dynamic_cast<TDerivative*>(v.get())) {
                return;
            }
        }
        values.push(std::make_unique<TDerivative>());
    }
};

class PressureGradient : public Abstract::Derivative {
private:
    ArrayView<const Float> p, rho, m;
    ArrayView<Vector> dv; // this is threadlocal

public:
    virtual void update(Storage& input, Storage& results) override {
        tie(p, rho, m) =
            input.getValues<Float>(QuantityIds::PRESSURE, QuantityIds::DENSITY, QuantityIds::MASS);
        dv = results.getAll<Vector>(QuantityIds::POSITIONS)[2];
    }

    void sum(const Size i, ArrayView<const NeighbourRecord> neighs, ArrayView<const Vector> grads) override {
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k].index;
            const Vector f = -(p[i] + p[j]) / (rho[i] * rho[j]) * grads[i];
            dv[i] += m[j] * f;
            dv[j] -= m[i] * f;
        }
    }
};

class VelocityDivergence : public Abstract::Derivative {
private:
    ArrayView<const Float> rho, m;
    ArrayView<Vector> v;
    ArrayView<Float> divv; // this is threadlocal

public:
    virtual void update(Storage& input, Storage& results) override {
        tie(rho, m) = input.getValues<Float>(QuantityIds::DENSITY, QuantityIds::MASS);
        v = input.getDt<Vector>(QuantityIds::POSITIONS);
        divv = results.getValue<Float>(QuantityIds::VELOCITY_DIVERGENCE);
    }

    void sum(const Size i, ArrayView<const NeighbourRecord> neighs, ArrayView<const Vector> grads) override {
        for (Size k = 0; k < neighs.size(); ++k) {
            const Size j = neighs[k].index;
            const Float proj = dot(v[i] - v[j], grads[i]);
            divv[i] += m[j] / rho[j] * proj;
            divv[j] += m[i] / rho[i] * proj;
        }
    }
};

class PressureForce {
private:
public:
    // sets up the force for single thread, will be called multiple times
    virtual void initializeThread(DerivativeHolder& derivatives) override {
        derivatives.require<PressureGradient>();
        derivatives.require<VelocityDivergence>();
    }

    void integrate() {
        e = p[i] / rho[i] * divv[i];
        du[i] += m[i] * e;
        du[j] += m[j] * e;
    }
};

NAMESPACE_SPH_END
