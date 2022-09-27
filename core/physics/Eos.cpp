#include "physics/Eos.h"
#include "math/Functional.h"
#include "physics/Constants.h"
#include "system/Settings.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

static Float bisectDensity(const IEos& eos, const Float p, const Float u, const Float rho0) {
    Float rhoMax = rho0;
    while (rhoMax < 1.e6_f * rho0) {
        rhoMax *= 2;
        if (eos.evaluate(rhoMax, u)[0] > p) {
            break;
        }
    }
    Float rhoMin = rho0;
    while (rhoMin > 1.e-6_f * rho0) {
        rhoMin *= 0.5;
        if (eos.evaluate(rhoMin, u)[0] < p) {
            break;
        }
    }

    auto func = [&eos, u, p0 = p](const Float rho) {
        Float p, cs;
        tie(p, cs) = eos.evaluate(rho, u);
        return p0 - p;
    };
    Optional<Float> root = getRoot(Interval(rhoMin, rhoMax), EPS, func);
    SPH_ASSERT(root);
    return root.value();
}

//-----------------------------------------------------------------------------------------------------------
// IdealGasEos implementation
//-----------------------------------------------------------------------------------------------------------

IdealGasEos::IdealGasEos(const Float gamma)
    : gamma(gamma) {}

Pair<Float> IdealGasEos::evaluate(const Float rho, const Float u) const {
    const Float p = (gamma - 1._f) * u * rho;
    return { p, sqrt(gamma * p / rho) };
}

Float IdealGasEos::getTemperature(const Float UNUSED(rho), const Float u) const {
    return u / Constants::gasConstant;
}

Float IdealGasEos::getInternalEnergy(const Float rho, const Float p) const {
    return p / ((gamma - 1._f) * rho);
}

Float IdealGasEos::getDensity(const Float p, const Float u) const {
    return p / ((gamma - 1._f) * u);
}

Float IdealGasEos::getSpecificEntropy(const Float rho, const Float p) const {
    return p / pow(rho, gamma);
}

//-----------------------------------------------------------------------------------------------------------
// Polytropic implementation
//-----------------------------------------------------------------------------------------------------------

PolytropicEos::PolytropicEos(const Float K, const Float gamma)
    : K(K)
    , gamma(gamma) {}

Pair<Float> PolytropicEos::evaluate(const Float rho, const Float UNUSED(u)) const {
    const Float p = K * pow(rho, gamma);
    return { p, sqrt(gamma * p / rho) };
}

Float PolytropicEos::getTemperature(const Float UNUSED(rho), const Float UNUSED(u)) const {
    NOT_IMPLEMENTED;
}

Float PolytropicEos::getInternalEnergy(const Float UNUSED(rho), const Float UNUSED(p)) const {
    NOT_IMPLEMENTED;
}

Float PolytropicEos::getDensity(const Float p, const Float UNUSED(u)) const {
    return pow(p / K, 1.f / gamma);
}

//-----------------------------------------------------------------------------------------------------------
// TaitEos implementation
//-----------------------------------------------------------------------------------------------------------

TaitEos::TaitEos(const BodySettings& settings) {
    c0 = settings.get<Float>(BodySettingsId::TAIT_SOUND_SPEED);
    rho0 = settings.get<Float>(BodySettingsId::DENSITY);
    gamma = settings.get<Float>(BodySettingsId::TAIT_GAMMA);
    c_p = settings.get<Float>(BodySettingsId::HEAT_CAPACITY);
}

Pair<Float> TaitEos::evaluate(const Float rho, const Float UNUSED(u)) const {
    const Float p = sqr(c0) * rho0 / gamma * (pow(rho / rho0, gamma) - 1._f);
    return { p, c0 };
}

Float TaitEos::getTemperature(const Float UNUSED(rho), const Float u) const {
    return u / c_p;
}

//-----------------------------------------------------------------------------------------------------------
// MieGruneisenEos implementation
//-----------------------------------------------------------------------------------------------------------

MieGruneisenEos::MieGruneisenEos(const BodySettings& settings) {
    c0 = settings.get<Float>(BodySettingsId::BULK_SOUND_SPEED);
    rho0 = settings.get<Float>(BodySettingsId::DENSITY);
    Gamma = settings.get<Float>(BodySettingsId::GRUNEISEN_GAMMA);
    s = settings.get<Float>(BodySettingsId::HUGONIOT_SLOPE);
    c_p = settings.get<Float>(BodySettingsId::HEAT_CAPACITY);
}

Pair<Float> MieGruneisenEos::evaluate(const Float rho, const Float u) const {
    const Float chi = 1._f - rho0 / rho;
    SPH_ASSERT(isReal(chi));
    const Float num = rho0 * sqr(c0) * chi * (1._f - 0.5_f * Gamma * chi);
    const Float denom = sqr(1._f - s * chi);
    SPH_ASSERT(denom != 0._f);
    return { num / denom + Gamma * u * rho, c0 }; /// \todo derive the sound speed
}

Float MieGruneisenEos::getTemperature(const Float UNUSED(rho), const Float u) const {
    return u / c_p;
}

//-----------------------------------------------------------------------------------------------------------
// TillotsonEos implementation
//-----------------------------------------------------------------------------------------------------------

TillotsonEos::TillotsonEos(const BodySettings& settings) {
    u0 = settings.get<Float>(BodySettingsId::TILLOTSON_SUBLIMATION);
    uiv = settings.get<Float>(BodySettingsId::TILLOTSON_ENERGY_IV);
    ucv = settings.get<Float>(BodySettingsId::TILLOTSON_ENERGY_CV);
    a = settings.get<Float>(BodySettingsId::TILLOTSON_SMALL_A);
    b = settings.get<Float>(BodySettingsId::TILLOTSON_SMALL_B);
    rho0 = settings.get<Float>(BodySettingsId::DENSITY);
    A = settings.get<Float>(BodySettingsId::BULK_MODULUS);
    B = settings.get<Float>(BodySettingsId::TILLOTSON_NONLINEAR_B);
    alpha = settings.get<Float>(BodySettingsId::TILLOTSON_ALPHA);
    beta = settings.get<Float>(BodySettingsId::TILLOTSON_BETA);
    c_p = settings.get<Float>(BodySettingsId::HEAT_CAPACITY);
}

Pair<Float> TillotsonEos::evaluate(const Float rho, const Float u) const {
    const Float eta = rho / rho0;
    const Float mu = eta - 1._f;
    const Float denom = u / (u0 * eta * eta) + 1._f;
    SPH_ASSERT(isReal(denom));
    SPH_ASSERT(isReal(eta));
    // compressed phase
    const Float pc = (a + b / denom) * rho * u + A * mu + B * mu * mu;
    Float dpdu = a * rho + b * rho / sqr(denom);
    Float dpdrho = a * u + b * u * (3._f * denom - 2._f) / sqr(denom) + A / rho0 + 2._f * B * mu / rho0;
    const Float csc = dpdrho + dpdu * pc / (rho * rho);
    SPH_ASSERT(isReal(csc));

    // expanded phase
    const Float rhoExp = rho0 / rho - 1._f;
    const Float betaExp = exp(-min(beta * rhoExp, 70._f));
    const Float alphaExp = exp(-min(alpha * sqr(rhoExp), 70._f));
    const Float pe = a * rho * u + (b * rho * u / denom + A * mu * betaExp) * alphaExp;
    dpdu = a * rho + alphaExp * b * rho / sqr(denom);
    dpdrho = a * u + alphaExp * (b * u * (3._f * denom - 2._f) / sqr(denom)) +
             alphaExp * (b * u * rho / denom) * rho0 * (2._f * alpha * rhoExp) / sqr(rho) +
             alphaExp * A * betaExp * (1._f / rho0 + rho0 * mu / sqr(rho) * (2._f * alpha * rhoExp + beta));
    Float cse = dpdrho + dpdu * pe / (rho * rho);
    SPH_ASSERT(isReal(cse));
    cse = max(cse, 0._f);

    // select phase based on internal energy
    Float p = pc, cs = csc;
    if (rho <= rho0 && u > ucv) {
        p = pe;
        cs = cse;
    } else if (rho <= rho0 && u > uiv && u <= ucv) {
        p = ((u - uiv) * pe + (ucv - u) * pc) / (ucv - uiv);
        cs = ((u - uiv) * cse + (ucv - u) * csc) / (ucv - uiv);
    }
    // clamp sound speed to prevent negative values
    cs = max(cs, 0.25_f * A / rho0);

    SPH_ASSERT(isReal(p) && isReal(cs) && cs > 0._f);
    return { p, sqrt(cs) };
}

Float TillotsonEos::getInternalEnergy(const Float rho, const Float p) const {
    // try compressed phase first
    const Float eta = rho / rho0;
    const Float mu = eta - 1._f;
    const Float x = (p - A * mu - B * sqr(mu)) / rho;
    const Float l = a;
    const Float m = u0 * sqr(eta) * (a + b) - x;
    const Float n = -x * u0 * sqr(eta);
    const Float u = (-m + sqrt(sqr(m) - 4._f * l * n)) / (2._f * l);
    SPH_ASSERT(isReal(u));

    if (rho <= rho0 && u > uiv) {
        // this is actually in expanded regime, find root
        auto func = [this, rho, p0 = p](const Float u) {
            Float p, cs;
            tie(p, cs) = this->evaluate(rho, u);
            return p0 - p;
        };
        /// \todo optimize, find proper upper bound
        Optional<Float> root = getRoot(Interval(0._f, u0), EPS, func);
        SPH_ASSERT(root);
        // if no root exists, return the compressed u to avoid crashing
        return root.valueOr(u);
    } else {
        return u;
    }
}

Float TillotsonEos::getDensity(const Float p, const Float u) const {
    // both phases are highly non-linear in density, no chance of getting an analytic solution ...
    // so let's find the root
    return bisectDensity(*this, p, u, rho0);
}

Float TillotsonEos::getTemperature(const Float UNUSED(rho), const Float u) const {
    return u / c_p;
}

//-----------------------------------------------------------------------------------------------------------
// SimplifiedTillotsonEos implementation
//-----------------------------------------------------------------------------------------------------------

SimplifiedTillotsonEos::SimplifiedTillotsonEos(const BodySettings& settings) {
    const Float a = settings.get<Float>(BodySettingsId::TILLOTSON_SMALL_A);
    const Float b = settings.get<Float>(BodySettingsId::TILLOTSON_SMALL_B);
    c = a + b;
    rho0 = settings.get<Float>(BodySettingsId::DENSITY);
    A = settings.get<Float>(BodySettingsId::BULK_MODULUS);
    c_p = settings.get<Float>(BodySettingsId::HEAT_CAPACITY);
}

Pair<Float> SimplifiedTillotsonEos::evaluate(const Float rho, const Float u) const {
    const Float mu = rho / rho0 - 1._f;
    const Float p = c * rho * u + A * mu;
    const Float cs = sqrt(A / rho0); /// \todo Correctly, has to depend on u!
    return { p, cs };
}

Float SimplifiedTillotsonEos::getTemperature(const Float UNUSED(rho), const Float u) const {
    return u / c_p;
}

//-----------------------------------------------------------------------------------------------------------
// HubbardMacFarlaneEos implementation
//-----------------------------------------------------------------------------------------------------------

static RegisterEnum<HubbardMacFarlaneEos::Type> sType({
    { HubbardMacFarlaneEos::Type::ROCK, "rock", "Suitable for rocky cores of planets." },
    { HubbardMacFarlaneEos::Type::ICE, "ice", "Suitable for icy mantles of planets." },
});

// http://etheses.dur.ac.uk/13349/1/thesis_jacob_kegerreis.pdf?DDD25+
HubbardMacFarlaneEos::HubbardMacFarlaneEos(const Type type, ArrayView<const Abundance> abundances)
    : type(type) {
    c_v = 0;
    const Float n = type == Type::ICE ? 2.1_f : 3._f;
    for (const Abundance& a : abundances) {
        c_v += a.fraction * a.numberOfAtoms * n * Constants::gasConstant / a.molarMass;
    }
    c_v *= 1000; // cgs -> si

    if (type == Type::ICE) {
        rho0 = 947.8_f;
        A = 2e9_f;
    } else {
        rho0 = 2704.8_f;
        A = 2e10_f;
    }

    //calculateTemperatureTable();
}

Pair<Float> HubbardMacFarlaneEos::evaluate(const Float rho, const Float u) const {
    /*/ const Float T = temperatureTable.interpolate(rho, u);
    //const Float T = 0;
    const Float p = evaluateFromDensityAndTemperature(rho, T);*/

    const Float rho_cgs = rho * 1.e-3_f;
    Float p0_Mbar;
    const float clampedRho = min(rho_cgs, 6._f);
    if (type == Type::ICE) {
        p0_Mbar = pow(rho_cgs, 4.067_f) * exp(-3.097_f - 0.228_f * clampedRho - 0.0102_f * sqr(clampedRho));
    } else {
        p0_Mbar = pow(rho_cgs, 14.563_f) * exp(-15.041_f - 2.130_f * clampedRho + 0.0483_f * sqr(clampedRho));
    }
    SPH_ASSERT(p0_Mbar > 0);
    const Float p = p0_Mbar * 1.e11_f + 4 * rho * u;

    const Float cs = sqrt(A / rho);
    return { p, cs };
}

Float HubbardMacFarlaneEos::getInternalEnergy(const Float rho, const Float p) const {
    NOT_IMPLEMENTED;
}

Float HubbardMacFarlaneEos::getDensity(const Float p, const Float u) const {
    // both phases are highly non-linear in density, no chance of getting an analytic solution ...
    // so let's find the root
    return bisectDensity(*this, p, u, rho0);
}

Float HubbardMacFarlaneEos::evaluateFromDensityAndTemperature(const Float rho, const Float T) const {
    const Float rho_cgs = rho * 1.e-3_f;
    Float p0_Mbar;
    const float clampedRho = min(rho_cgs, 5._f);
    if (type == Type::ICE) {
        p0_Mbar = pow(rho_cgs, 4.067_f) * exp(-3.097_f - 0.228_f * clampedRho - 0.0102_f * sqr(clampedRho));
    } else {
        p0_Mbar = pow(rho_cgs, 14.563_f) * exp(-15.041_f - 2.130_f * clampedRho + 0.0483_f * sqr(clampedRho));
    }
    SPH_ASSERT(p0_Mbar > 0);
    return p0_Mbar * 1.e11_f + c_v * rho * T;
}

void HubbardMacFarlaneEos::calculateTemperatureTable() {
    const Float rho_min = rho0;
    const Float rho_max = 5e4;
    const Float rho_step = 1.02_f;
    const Float T_min = 1;
    const Float T_max = 1e6;
    const Float T_step = 1.25_f;
    Array<Float> rhos;
    for (Float rho = rho_min; rho < rho_max; rho *= rho_step) {
        rhos.push(rho);
    }
    Array<Float> Ts;
    for (Float T = T_min; T < T_max; T *= T_step) {
        Ts.push(T);
    }
    Lut2D<Float> energyTable(rhos.size(), Ts.size(), rhos.clone(), std::move(Ts));
    Float u_max = 0;
    std::ofstream ofs(format("temperature_{}.txt", EnumMap::toString(type)).toAscii());
    Size i = 0;
    for (Float rho = rho_min; rho < rho_max; rho *= rho_step, ++i) {
        const Float drho = rho * (rho_step - 1);
        Size j = 0;
        Float u = 0;
        for (Float T = T_min; T < T_max; T *= T_step, ++j) {
            const Float dT = T * (T_step - 1);
            const Float P = evaluateFromDensityAndTemperature(rho, T);
            const Float du = c_v * dT - (T * c_v * rho - P) * drho / sqr(rho);
            u += du;
            energyTable.at(i, j) = u;
            u_max = max(u_max, u);
        }
        
    }
    Array<Float> us;
    for (Float u = 1; u < u_max;  u *= T_step) {
        us.push(u);
    }
    temperatureTable = Lut2D<Float>(rhos.size(), us.size(), std::move(rhos), std::move(us));
    for (Size i = 0; i < temperatureTable.getValuesX().size(); ++i) {
        for (Size j = 0; j < temperatureTable.getValuesY().size(); ++j) {
            const Float rho = temperatureTable.getValuesX()[i];
            const Float u = temperatureTable.getValuesY()[j];
            ArrayView<const Float> Ts = energyTable.getValuesY();
            temperatureTable.at(i, j) = T_max;
            for (Size k = 0; k < Ts.size(); ++k) {
                if (energyTable.interpolate(rho, Ts[k]) > u) {
                    temperatureTable.at(i, j) = Ts[k];
                }
            }
            ofs << rho << " " << u << " " << temperatureTable.at(i, j) << "\n";
        }
        ofs << "\n";
    }
}

//-----------------------------------------------------------------------------------------------------------
// MurnaghanEos implementation
//-----------------------------------------------------------------------------------------------------------

MurnaghanEos::MurnaghanEos(const BodySettings& settings) {
    rho0 = settings.get<Float>(BodySettingsId::DENSITY);
    A = settings.get<Float>(BodySettingsId::BULK_MODULUS);
    c_p = settings.get<Float>(BodySettingsId::HEAT_CAPACITY);
}

Pair<Float> MurnaghanEos::evaluate(const Float rho, const Float UNUSED(u)) const {
    const Float cs = sqrt(A / rho0);
    const Float p = sqr(cs) * (rho - rho0);
    return { p, cs };
}

Float MurnaghanEos::getTemperature(const Float UNUSED(rho), const Float u) const {
    return u / c_p;
}

Lut<Float> computeAdiabat(const IEos& eos, const Interval& range, float u0, Size resolution) {
    // For adiabat, ds=0, this du/d(rho) = P/rho^2
    Float u = u0;
    Float drho = range.size() / resolution;
    Array<Float> us;
    for (Float rho = range.lower(); rho < range.upper(); rho += drho) {
        const Float P = eos.evaluate(rho, u)[0];
        const Float dudrho = max(P, 0._f) / sqr(rho);
        u += dudrho * drho;
        SPH_ASSERT(u >= 0.f);
        us.push(u);
    }
    return Lut<Float>(range, std::move(us));
}

NAMESPACE_SPH_END
