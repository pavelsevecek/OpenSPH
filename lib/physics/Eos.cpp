#include "physics/Eos.h"
#include "physics/Constants.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

IdealGasEos::IdealGasEos(const Float gamma)
    : gamma(gamma) {}

Tuple<Float, Float> IdealGasEos::evaluate(const Float rho, const Float u) const {
    const Float p = (gamma - 1._f) * u * rho;
    return { p, sqrt(gamma * p / rho) };
}

Float IdealGasEos::getInternalEnergy(const Float rho, const Float p) const {
    return p / ((gamma - 1._f) * rho);
}

Float IdealGasEos::getTemperature(const Float u) const {
    return u / Constants::gasConstant;
}


TillotsonEos::TillotsonEos(const BodySettings& settings)
    : u0(settings.get<Float>(BodySettingsIds::TILLOTSON_SUBLIMATION))
    , uiv(settings.get<Float>(BodySettingsIds::TILLOTSON_ENERGY_IV))
    , ucv(settings.get<Float>(BodySettingsIds::TILLOTSON_ENERGY_CV))
    , a(settings.get<Float>(BodySettingsIds::TILLOTSON_SMALL_A))
    , b(settings.get<Float>(BodySettingsIds::TILLOTSON_SMALL_B))
    , rho0(settings.get<Float>(BodySettingsIds::DENSITY))
    , A(settings.get<Float>(BodySettingsIds::BULK_MODULUS))
    , B(settings.get<Float>(BodySettingsIds::TILLOTSON_NONLINEAR_B))
    , alpha(settings.get<Float>(BodySettingsIds::TILLOTSON_ALPHA))
    , beta(settings.get<Float>(BodySettingsIds::TILLOTSON_BETA)) {}

Tuple<Float, Float> TillotsonEos::evaluate(const Float rho, const Float u) const {
    const Float eta = rho / rho0;
    const Float mu = eta - 1._f;
    const Float denom = u / (u0 * eta * eta) + 1._f;
    ASSERT(isReal(denom));
    ASSERT(isReal(eta));
    // compressed phase
    const Float pc = (a + b / denom) * rho * u + A * mu + B * mu * mu;
    Float dpdu = a * rho + b * rho / sqr(denom);
    Float dpdrho = a * u + b * u * (3._f * denom - 2._f) / sqr(denom) + A / rho0 + 2._f * B * mu / rho0;
    const Float csc = dpdrho + dpdu * pc / (rho * rho);
    ASSERT(isReal(csc));

    // expanded phase
    const Float rhoExp = rho0 / rho - 1._f;
    ASSERT(beta * rhoExp < 70._f);
    const Float betaExp = exp(-beta * rhoExp);
    const Float alphaExp = exp(-alpha * sqr(rhoExp));
    ASSERT(alpha * sqr(rhoExp) < 70._f);
    const Float pe = a * rho * u + (b * rho * u / denom + A * mu * betaExp) * alphaExp;
    dpdu = a * rho + alphaExp * b * rho / sqr(denom);
    dpdrho = a * u + alphaExp * (b * u * (3._f * denom - 2._f) / sqr(denom)) +
             alphaExp * (b * u * rho / denom) * rho0 * (2._f * alpha * rhoExp) / sqr(rho) +
             alphaExp * A * betaExp * (1._f / rho0 + rho0 * mu / sqr(rho) * (2._f * alpha * rhoExp + beta));
    Float cse = dpdrho + dpdu * pe / (rho * rho);
    ASSERT(isReal(cse));
    cse = max(cse, 0._f);

    // select phase based on internal energy
    Float p = pc, cs = csc;
    if (rho <= rho0 && u > ucv) {
        p = pe;
        cs = cse;
    } else if (rho <= rho0 && u > uiv && u <= ucv) {
        p = ((u - uiv) * pe + (ucv - u) * pc) / (ucv - uiv);
        /// \todo interpolate squared values or after sqrt?
        cs = ((u - uiv) * cse + (ucv - u) * csc) / (ucv - uiv);
    }
    // clamp sound speed to prevent negative values
    cs = max(cs, 0.25_f * A / rho0);

    ASSERT(isReal(p) && isReal(cs) && cs > 0._f);
    return { p, sqrt(cs) };
}


MurnaghanEos::MurnaghanEos(const BodySettings& settings)
    : rho0(settings.get<Float>(BodySettingsIds::DENSITY))
    , A(settings.get<Float>(BodySettingsIds::BULK_MODULUS)) {}

Tuple<Float, Float> MurnaghanEos::evaluate(const Float rho, const Float UNUSED(u)) const {
    const Float cs = sqrt(A / rho0);
    const Float p = sqr(cs) * (rho - rho0);
    return { p, cs };
}


NAMESPACE_SPH_END
