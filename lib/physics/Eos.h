#pragma once

/// Equations of state.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/Array.h"
#include "objects/containers/Tuple.h"
#include "physics/Constants.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN


namespace Abstract {
    /// Base class for equations of state.
    class Eos : public Polymorphic {
    public:
        /// Computes pressure and local sound speed from given density rho and specific internal energy u.
        virtual Tuple<Float, Float> getPressure(const Float rho, const Float u) const = 0;

        /// Inverted function; computes specific internal energy u from given density rho and pressure p.
        virtual Float getInternalEnergy(const Float rho, const Float p) const = 0;
    };
}

/// Equation of state for ideal gas.
class IdealGasEos : public Abstract::Eos {
private:
    const Float gamma;

public:
    IdealGasEos(const Float gamma)
        : gamma(gamma) {}

    virtual Tuple<Float, Float> getPressure(const Float rho, const Float u) const override {
        const Float p = (gamma - 1._f) * u * rho;
        return makeTuple(p, Math::sqrt(gamma * p / rho));
    }

    virtual Float getInternalEnergy(const Float rho, const Float p) const override {
        return p / ((gamma - 1._f) * rho);
    }

    Float getTemperature(const Float u) { return u / Constants::gasConstant; }
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
    TillotsonEos(const BodySettings& settings)
        : u0(settings.get<Float>(BodySettingsIds::ENERGY))
        , uiv(settings.get<Float>(BodySettingsIds::TILLOTSON_ENERGY_IV))
        , ucv(settings.get<Float>(BodySettingsIds::TILLOTSON_ENERGY_CV))
        , a(settings.get<Float>(BodySettingsIds::TILLOTSON_SMALL_A))
        , b(settings.get<Float>(BodySettingsIds::TILLOTSON_SMALL_B))
        , rho0(settings.get<Float>(BodySettingsIds::DENSITY))
        , A(settings.get<Float>(BodySettingsIds::BULK_MODULUS))
        , B(settings.get<Float>(BodySettingsIds::TILLOTSON_NONLINEAR_B))
        , alpha(settings.get<Float>(BodySettingsIds::TILLOTSON_ALPHA))
        , beta(settings.get<Float>(BodySettingsIds::TILLOTSON_BETA)) {}

    virtual Tuple<Float, Float> getPressure(const Float rho, const Float u) const override {
        const Float eta   = rho / rho0;
        const Float mu    = eta - 1._f;
        const Float denom = u / (u0 * eta * eta) + 1._f;
        ASSERT(Math::isReal(denom));
        ASSERT(Math::isReal(eta));
        // compressed phase
        const Float pc = (a + b / denom) * rho * u + A * mu + B * mu * mu;
        Float dpdu     = a * rho + b * rho / Math::sqr(denom);
        Float dpdrho =
            a * u + b * u * (3._f * denom - 2._f) / Math::sqr(denom) + A / rho0 + 2._f * B * mu / rho0;
        const Float csc = dpdrho + dpdu * pc / (rho * rho);
        ASSERT(Math::isReal(csc));

        // expanded phase
        const Float rhoExp   = rho0 / rho - 1._f;
        const Float betaExp  = Math::exp(-beta * rhoExp);
        const Float alphaExp = Math::exp(-alpha * Math::sqr(rhoExp));
        const Float pe = a * rho * u + (b * rho * u / denom + A * mu * betaExp) * alphaExp;
        dpdu = a * rho + alphaExp * b * rho / Math::sqr(denom);
        dpdrho =
            a * u + alphaExp * (b * u * (3._f * denom - 2._f) / Math::sqr(denom)) +
            alphaExp * (b * u * rho / denom) * rho0 * (2._f * alpha * rhoExp) / Math::sqr(rho) +
            alphaExp * A * betaExp * (1._f / rho0 + rho0 * mu / Math::sqr(rho) * (2._f * alpha * rhoExp + beta));
        const Float cse = dpdrho + dpdu * pe / (rho * rho);
        ASSERT(Math::isReal(cse));

        // select phase based on internal energy
        Float p, cs;
        if (u < uiv || rho > rho0) {
            p  = pc;
            cs = csc;
        } else if (rho <= rho0 && u > ucv) {
            p  = pe;
            cs = cse;
        } else if (rho <= rho0) {
            p  = ((u - uiv) * pe + (ucv - u) * pc) / (ucv - uiv);
            cs = ((u - uiv) * cse + (ucv - u) * csc) / (ucv - uiv);
        }
        ASSERT(Math::isReal(p) && Math::isReal(cs));
        return makeTuple(p, cs);
    }

    virtual Float getInternalEnergy(const Float UNUSED(rho), const Float UNUSED(p)) const override {
        NOT_IMPLEMENTED;
    }
};

NAMESPACE_SPH_END
