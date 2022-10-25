#include "sph/initial/Equilibrium.h"
#include "math/Functional.h"
#include "physics/Constants.h"
#include "physics/Eos.h"

NAMESPACE_SPH_BEGIN

Lut<Float> integrateMassProfile(const Lut<Float>& densityProfile) {
    Array<Float> massProfile;
    Float M_tot = 0;
    const Float dr = densityProfile.getRange().size() / densityProfile.size();
    for (const auto& value : densityProfile) {
        const Float r = value.x;
        const Float rho = value.y;
        M_tot += sphereSurfaceArea(r) * rho * dr;
        massProfile.push(M_tot);
    }
    return Lut<Float>(densityProfile.getRange(), std::move(massProfile));
}

Lut<Float> integratePressureProfile(const Lut<Float>& densityProfile) {
    const Interval radialRange = densityProfile.getRange();
    const Float dr = radialRange.size() / densityProfile.size();
    const Lut<Float> massProfile = integrateMassProfile(densityProfile);

    Float p0 = 0.f;
    Array<Float> pressures(densityProfile.size());
    for (int i = densityProfile.size() - 1; i >= 0; --i) {
        const Float r = densityProfile.valueAtIndex(i).x;
        const Float rho = densityProfile.valueAtIndex(i).y;
        const Float M = massProfile.valueAtIndex(i).y;
        Float p = p0;
        if (r > 0) {
            // avoid the singularity at the center
            p += Constants::gravity * M * rho / sqr(r) * dr;
        }
        SPH_ASSERT(isReal(p));
        pressures[i] = p;
        p0 = p;
    }

    return Lut<Float>(radialRange, std::move(pressures));
}

static PlanetaryProfile integrateRadialProfiles(ArrayView<const RadialShell> shells,
    ArrayView<Lut<Float>> adiabats,
    ArrayView<const Size> shellIndices,
    const Lut<Float>& densityGuess,
    const Interval& densityRange) {
    Interval radialRange = densityGuess.getRange();
    Lut<Float> massProfile = integrateMassProfile(densityGuess);
    const Float radius = massProfile.getRange().upper();
    const Float dr = radialRange.size() / massProfile.size();

    Float p0 = 0;
    Float u0 = 0;
    // surface density
    Float rho0 = densityGuess(radius);

    Array<Float> density(massProfile.size());
    Array<Float> pressure(massProfile.size());
    Array<Float> energy(massProfile.size());

    for (int i = massProfile.size() - 1; i >= 0; --i) {
        density[i] = rho0;
        pressure[i] = p0;
        energy[i] = u0;

        if (i == 0) {
            break;
        }

        const auto value = massProfile.valueAtIndex(i);
        const Float r = value.x;
        const Float M = value.y;

        const IEos& eos = *shells[shellIndices[i]].eos;
        const Lut<Float>& adiabat = adiabats[shellIndices[i]];

        // why 1.33333 though??
        Float p = p0 + /*1.3333f */ Constants::gravity * M * rho0 / sqr(r) * dr;
        Float rho = getRoot(densityRange, 1.e-6f, [&](Float rho) {
            return eos.evaluate(rho, u0)[0] - p;
        }).valueOr(densityRange.upper());
        SPH_ASSERT(rho > 0);

        u0 = adiabat(rho);
        rho0 = rho;
        p0 = p;
    }
    PlanetaryProfile profiles;
    profiles.rho = Lut<Float>(radialRange, std::move(density));
    profiles.p = Lut<Float>(radialRange, std::move(pressure));
    profiles.u = Lut<Float>(radialRange, std::move(energy));

    return profiles;
}

PlanetaryProfile computeEquilibriumRadialProfile(ArrayView<const RadialShell> shells,
    const Lut<Float>& densityGuess,
    const EquilibriumParams& params) {
    Interval densityRange = densityGuess.getValueRange();
    densityRange = Interval(densityRange.lower(), 10 * densityRange.upper());

    Array<Lut<Float>> adiabats(shells.size());
    switch (params.temperatureProfile) {
    case TemperatureProfileEnum::ADIABATIC:
        for (Size i = 0; i < shells.size(); ++i) {
            adiabats[i] = computeAdiabat(*shells[i].eos, densityRange, 0, 100000);
        }
        break;
    case TemperatureProfileEnum::ISOTHERMAL:
        for (Size i = 0; i < shells.size(); ++i) {
            adiabats[i] = Lut<Float>(densityRange, 1000, [](Float) { return 0._f; });
        }
        break;
    default:
        NOT_IMPLEMENTED;
    }

    Array<Size> shellIndices(densityGuess.size());
    shellIndices.fill(Size(-1));
    for (Size i = 0; i < shellIndices.size(); ++i) {
        const Float r = densityGuess.valueAtIndex(i).x;
        for (Size j = 0; j < shells.size(); ++j) {
            if (shells[j].radialRange.contains(r)) {
                shellIndices[i] = j;
                break;
            }
        }
        SPH_ASSERT(shellIndices[i] != Size(-1));
    }

    PlanetaryProfile profiles;
    profiles.rho = densityGuess;

    for (Size iter = 0; iter < params.iterCnt; ++iter) {
        profiles = integrateRadialProfiles(shells, adiabats, shellIndices, profiles.rho, densityRange);
    }

    return profiles;
}

NAMESPACE_SPH_END
