#pragma once

/// \file Eos.h
/// \brief Equations of state
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/ForwardDecl.h"
#include "common/Globals.h"
#include "objects/containers/Array.h"
#include "objects/containers/StaticArray.h"

NAMESPACE_SPH_BEGIN


namespace Abstract {
    /// Base class for equations of state.
    class Eos : public Polymorphic {
    public:
        /// Computes pressure and local sound speed from given density rho and specific internal energy u.
        virtual Pair<Float> evaluate(const Float rho, const Float u) const = 0;

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

    virtual Pair<Float> evaluate(const Float rho, const Float u) const override;

    virtual Float getInternalEnergy(const Float rho, const Float p) const override;

    Float getTemperature(const Float u) const;
};

/// Tillotson equation of state \cite Tillotson_1962
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

    virtual Pair<Float> evaluate(const Float rho, const Float u) const override;

    /// Currently not implemented.
    virtual Float getInternalEnergy(const Float UNUSED(rho), const Float UNUSED(p)) const override {
        NOT_IMPLEMENTED;
    }
};

/// Murnaghan equation of state. Pressure is computed from density only (does not depend on energy).
class MurnaghanEos : public Abstract::Eos {
private:
    Float rho0;
    Float A;

public:
    MurnaghanEos(const BodySettings& settings);

    virtual Pair<Float> evaluate(const Float rho, const Float u) const override;

    /// Currently not implemented.
    virtual Float getInternalEnergy(const Float UNUSED(rho), const Float UNUSED(p)) const override {
        NOT_IMPLEMENTED;
    }
};


NAMESPACE_SPH_END
