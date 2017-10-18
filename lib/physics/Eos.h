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

/// \brief Base class for equations of state.
class IEos : public Polymorphic {
public:
    /// Computes pressure and local sound speed from given density rho and specific internal energy u.
    virtual Pair<Float> evaluate(const Float rho, const Float u) const = 0;

    /// Inverted function; computes specific internal energy u from given density rho and pressure p.
    virtual Float getInternalEnergy(const Float rho, const Float p) const = 0;

    /// Inverted function; computes density from pressure p and internal energy u.
    virtual Float getDensity(const Float p, const Float u) const = 0;
};


/// Equation of state for ideal gas.
class IdealGasEos : public IEos {
private:
    const Float gamma;

public:
    explicit IdealGasEos(const Float gamma);

    virtual Pair<Float> evaluate(const Float rho, const Float u) const override;

    virtual Float getInternalEnergy(const Float rho, const Float p) const override;

    virtual Float getDensity(const Float p, const Float u) const override;

    Float getTemperature(const Float u) const;
};

/// \brief Tait equation of state
///
/// Equation describing behavior of water and other fluids. Depends only on density, does not require energy
/// equation to be used.
class TaitEos : public IEos {
private:
    Float c0;    ///< Sound speed at reference density
    Float rho0;  ///< Reference density
    Float gamma; ///< Density exponent

public:
    explicit TaitEos(const BodySettings& settings);

    virtual Pair<Float> evaluate(const Float rho, const Float u) const override;

    virtual Float getInternalEnergy(const Float UNUSED(rho), const Float UNUSED(p)) const override {
        NOT_IMPLEMENTED;
    }

    virtual Float getDensity(const Float UNUSED(p), const Float UNUSED(u)) const override {
        NOT_IMPLEMENTED;
    }
};

/// \brief Mie-Gruneisen equation of state
///
/// Simple equation of state describing solids. It consists of two separated terms, first (reference curve)
/// depending solely on pressure. Uses extension of R. Menikoff \cite Menikoff_1962 which makes the equation
/// of state complete; it is therefore possible to compute temperature from known internal energy.
class MieGruneisenEos : public IEos {
private:
    Float c0;    ///< Bulk sound speed
    Float rho0;  ///< Reference density
    Float Gamma; ///< Gruneisen Gamma
    Float s;     ///< Linear Hugoniot slope coefficient

public:
    explicit MieGruneisenEos(const BodySettings& settings);

    virtual Pair<Float> evaluate(const Float rho, const Float u) const override;

    virtual Float getInternalEnergy(const Float UNUSED(rho), const Float UNUSED(p)) const override {
        NOT_IMPLEMENTED;
    }

    virtual Float getDensity(const Float UNUSED(p), const Float UNUSED(u)) const override {
        NOT_IMPLEMENTED;
    }
};

/// \brief Tillotson equation of state \cite Tillotson_1962.
///
/// Describes a behavior of a solid material in both the compressed and expanded phase. The phase transition
/// is defined by two parameters - energy u_iv of incipient vaporization and energy u_cv of complete
/// vaporization. Between these two values (and if the density is lower than the reference density rho_0,
/// meaning the material is not compressed), the pressure and sound speed is given by simple linear
/// interpolation between the two phases
///
/// Note that Tillotson equation is an incomplete equation of state, meaning no relation between temperature
/// and internal energy is described. If one needs to compute the temperature, more complex equation of state
/// has to be used.
class TillotsonEos : public IEos {
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
    explicit TillotsonEos(const BodySettings& settings);

    virtual Pair<Float> evaluate(const Float rho, const Float u) const override;

    virtual Float getInternalEnergy(const Float rho, const Float p) const override;

    /// \todo this is currently fine tuned for getting density in stationary (initial) state
    virtual Float getDensity(const Float p, const Float u) const override;
};

/// Murnaghan equation of state. Pressure is computed from density only (does not depend on energy).
class MurnaghanEos : public IEos {
private:
    Float rho0;
    Float A;

public:
    explicit MurnaghanEos(const BodySettings& settings);

    virtual Pair<Float> evaluate(const Float rho, const Float u) const override;

    /// Currently not implemented.
    virtual Float getInternalEnergy(const Float UNUSED(rho), const Float UNUSED(p)) const override {
        NOT_IMPLEMENTED;
    }

    /// Currently not implemented.
    virtual Float getDensity(const Float UNUSED(p), const Float UNUSED(u)) const override {
        NOT_IMPLEMENTED;
    }
};


NAMESPACE_SPH_END
