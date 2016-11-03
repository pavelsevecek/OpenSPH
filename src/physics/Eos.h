#pragma once

/// Equations of state.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/Array.h"
#include "physics/Constants.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN


namespace Abstract {
    /// Base class for equations of state.
    class Eos : public Polymorphic {
    public:
        /// Computes a pressure from given density.
        /// \todo EoS have more general relations, like P = P(rho, T) or P = P(rho, u)
        /// \todo maybe pass model as a parameter?
        virtual void getPressure(ArrayView<const Float> rho,
                                 ArrayView<const Float> u,
                                 ArrayView<Float> p) = 0;
    };
}

/// Equation of state for ideal gas.
class IdealGasEos : public Abstract::Eos {
private:
    const Float gamma;

public:
    IdealGasEos(const Float gamma = 1.5_f)
        : gamma(gamma) {}

    virtual void getPressure(ArrayView<const Float> rho,
                             ArrayView<const Float> u,
                             ArrayView<Float> p) override {
        ASSERT(rho.size() == u.size() && rho.size() == p.size());
        for (int i = 0; i < p.size(); ++i) {
            p[i] = gamma * u[i] * rho[i];
        }
    }

    Float getTemperature(const Float u) { return u / Constants::gasConstant; }
};

struct TillotsonParams : public Object {
    Float u0;
    Float uiv;
    Float ucv;
    Float a;
    Float b;
    Float rho0;
    /// ...
};

class TillotsonEos : public Abstract::Eos {
private:
    Float u0;
    Float uiv;
    Float ucv;
    Float a;
    Float b;
    Float rho0;
    Float A;
    Float B;
    Float alpha;
    Float beta;

public:
    TillotsonEos(const Settings<BodySettingsIds>& settings)
        : uiv(settings.get<Float>(BodySettingsIds::ENERGY_IV).get()) {}

    virtual void getPressure(ArrayView<const Float> rho,
                             ArrayView<const Float> u,
                             ArrayView<Float> p) override {
        ASSERT(rho.size() == u.size() && rho.size() == p.size());
        for (int i = 0; i < p.size(); ++i) {
            const Float eta   = rho[i] / rho0;
            const Float mu    = eta - 1._f;
            const Float denom = u[i] / (u0 * eta * eta) + 1._f;
            Float pc          = (a + b / denom) * rho[i] * u[i] + A * mu + B * mu * mu;

            Float rhoExp = rho0 / rho[i] - 1._f;
            Float pe     = a * rho[i] * u[i] +
                       (b * rho[i] * u[i] / denom + A * mu * Math::exp(-beta * rhoExp)) *
                           Math::exp(-alpha * Math::sqr(rhoExp));
            if (u[i] < uiv) {
                p[i] = pc;
            } else if (u[i] > ucv) {
                p[i] = pe;
            } else {
                p[i] = ((u[i] - uiv) * pe + (ucv - u[i]) * pc) / (ucv - uiv);
            }
        }
    }
};

NAMESPACE_SPH_END
