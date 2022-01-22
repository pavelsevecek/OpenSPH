#pragma once

#include "objects/wrappers/Lut2D.h"
#include "physics/Eos.h"

NAMESPACE_SPH_BEGIN

class Path;

/// \brief State quantities for given value of temperature and density.
struct EosTabValue {
    Float u;
    Float P;
    Float cs;

    EosTabValue operator+(const EosTabValue& other) const {
        return { u + other.u, P + other.P, cs + other.cs };
    }

    EosTabValue operator*(const Float f) const {
        return { u * f, P * f, cs * f };
    }
};

/// \brief Reads ANEOS material file and returns the contents as a look-up table, mapping a paif of values
/// (temperature, density) to corresponding values (specific energy, pressure, sound speed).
Lut2D<EosTabValue> parseAneosFile(const Path& path);

/// \brief Finds the initial density of the material from given look-up table.
Float getInitialDensity(const Lut2D<EosTabValue>& lut);

/// \brief ANEOS equation defined by a look-up table.
class Aneos : public IEos {
public:
    struct TabValue {
        Float P;
        Float cs;
        Float T;

        TabValue operator+(const TabValue& other) const;
        TabValue operator*(const Float f) const;
    };

private:
    Lut2D<TabValue> lut;

public:
    explicit Aneos(const Path& path);

    virtual Pair<Float> evaluate(const Float rho, const Float u) const override;

    virtual Float getTemperature(const Float rho, const Float u) const override;

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
