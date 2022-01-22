#pragma once

#include "objects/wrappers/Lut2D.h"
#include "physics/Eos.h"

NAMESPACE_SPH_BEGIN

class Path;

/// \brief ANEOS equation defined by a look-up table.
class Aneos : public IEos {
private:
    struct TabValue {
        Float P;
        Float cs;
        Float T;

        TabValue operator+(const TabValue& other) const;
        TabValue operator*(const Float f) const;
    };

    Lut2D<TabValue> lut;

public:
    explicit Aneos(const BodySettings& settings);

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

private:
    static Lut2D<TabValue> parseAneosFile(IScheduler& scheduler, const Path& path);
};

NAMESPACE_SPH_END
