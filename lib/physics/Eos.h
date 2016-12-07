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
        /// Computes pressure from given density rho and specific internal energy u.
        virtual Float getPressure(const Float rho, const Float u) const = 0;

        /// Inverted function; computes specific internal energy u from given density rho and pressure p.
        virtual Float getInternalEnergy(const Float rho, const Float p) const = 0;

        /// Computes local sound speed.
        virtual Float getSoundSpeed(const Float rho, const Float p) const = 0;
    };
}

/// Equation of state for ideal gas.
class IdealGasEos : public Abstract::Eos {
private:
    const Float gamma;

public:
    IdealGasEos(const Float gamma)
        : gamma(gamma) {}

    virtual Float getPressure(const Float rho, const Float u) const override {
        return (gamma - 1._f) * u * rho;
    }

    virtual Float getInternalEnergy(const Float rho, const Float p) const override {
        return p / ((gamma - 1._f) * rho);
    }

    virtual Float getSoundSpeed(const Float rho, const Float p) const override {
        return Math::sqrt(gamma * p / rho);
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
        : uiv(settings.get<Float>(BodySettingsIds::TILLOTSON_ENERGY_IV)) {}

        virtual Float getPressure(const Float UNUSED(rho), const Float UNUSED(u)) const override {
        NOT_IMPLEMENTED;
        /*ASSERT(rho.size() == u.size() && rho.size() == p.size());
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
        }*/
    }

    virtual Float getInternalEnergy(const Float UNUSED(rho), const Float UNUSED(p)) const override {
        NOT_IMPLEMENTED;
    }

     virtual Float getSoundSpeed(const Float UNUSED(rho), const Float UNUSED(p)) const override {
        NOT_IMPLEMENTED;
    }
};

NAMESPACE_SPH_END
