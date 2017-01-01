#include "physics/Eos.h"
#include "physics/Constants.h"

NAMESPACE_SPH_BEGIN

IdealGasEos::IdealGasEos(const Float gamma)
    : gamma(gamma) {}

Tuple<Float, Float> IdealGasEos::getPressure(const Float rho, const Float u) const {
    const Float p = (gamma - 1._f) * u * rho;
    return { p, Math::sqrt(gamma * p / rho) };
}

Float IdealGasEos::getInternalEnergy(const Float rho, const Float p) const {
    return p / ((gamma - 1._f) * rho);
}

Float IdealGasEos::getTemperature(const Float u) const {
    return u / Constants::gasConstant;
}


TillotsonEos::TillotsonEos(const BodySettings& settings)
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

Tuple<Float, Float> TillotsonEos::getPressure(const Float rho, const Float u) const {
    const Float eta = rho / rho0;
    const Float mu = eta - 1._f;
    const Float denom = u / (u0 * eta * eta) + 1._f;
    ASSERT(Math::isReal(denom));
    ASSERT(Math::isReal(eta));
    // compressed phase
    const Float pc = (a + b / denom) * rho * u + A * mu + B * mu * mu;
    Float dpdu = a * rho + b * rho / Math::sqr(denom);
    Float dpdrho = a * u + b * u * (3._f * denom - 2._f) / Math::sqr(denom) + A / rho0 + 2._f * B * mu / rho0;
    const Float csc = dpdrho + dpdu * pc / (rho * rho);
    ASSERT(Math::isReal(csc));

    // expanded phase
    const Float rhoExp = rho0 / rho - 1._f;
    const Float betaExp = Math::exp(-beta * rhoExp);
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
    Float p = pc, cs = csc;
    if (rho <= rho0 && u > ucv) {
        p = pe;
        cs = cse;
    } else if (rho <= rho0 && u > uiv && u <= ucv) {
        p = ((u - uiv) * pe + (ucv - u) * pc) / (ucv - uiv);
        cs = ((u - uiv) * cse + (ucv - u) * csc) / (ucv - uiv);
    }
    ASSERT(Math::isReal(p) && Math::isReal(cs));
    return { p, cs };
}

NAMESPACE_SPH_END
