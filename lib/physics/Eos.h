#pragma once

/// Equations of state.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/Array.h"
#include "objects/containers/Tuple.h"
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
    IdealGasEos(const Float gamma);

    virtual Tuple<Float, Float> getPressure(const Float rho, const Float u) const override;

    virtual Float getInternalEnergy(const Float rho, const Float p) const override;

    Float getTemperature(const Float u) const;
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
    TillotsonEos(const BodySettings& settings);

    virtual Tuple<Float, Float> getPressure(const Float rho, const Float u) const override;

    virtual Float getInternalEnergy(const Float UNUSED(rho), const Float UNUSED(p)) const override {
        NOT_IMPLEMENTED;
    }
};

NAMESPACE_SPH_END
