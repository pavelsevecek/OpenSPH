#pragma once

#include "objects/wrappers/Lut.h"

NAMESPACE_SPH_BEGIN

class IEos;

Lut<Float> integrateMassProfile(const Lut<Float>& densityProfile);

Lut<Float> integratePressureProfile(const Lut<Float>& densityProfile);

struct PlanetaryProfile {
    Lut<Float> rho;
    Lut<Float> u;
    Lut<Float> p;
};

enum class TemperatureProfileEnum {
    ISOTHERMAL,
    ADIABATIC,
};

struct EquilibriumParams {
    TemperatureProfileEnum temperatureProfile = TemperatureProfileEnum::ADIABATIC;
    Size iterCnt = 100;
};

struct RadialShell {
    const IEos* eos;
    Float referenceDensity;
    Interval radialRange;
};

PlanetaryProfile computeEquilibriumRadialProfile(ArrayView<const RadialShell> shells,
    const Lut<Float>& densityGuess,
    const EquilibriumParams& params = {});

NAMESPACE_SPH_END
