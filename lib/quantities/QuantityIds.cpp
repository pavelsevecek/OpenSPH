#include "quantities/QuantityIds.h"
#include "objects/utility/EnumMap.h"

NAMESPACE_SPH_BEGIN

QuantityMetadata::QuantityMetadata(const std::string& fullName,
    const std::wstring& label,
    const ValueEnum type,
    const std::string& dtName,
    const std::string& d2tName)
    : quantityName(fullName)
    , label(label)
    , expectedType(type) {
    if (!dtName.empty()) {
        derivativeName = dtName;
    } else {
        derivativeName = quantityName + " derivative";
    }

    if (!d2tName.empty()) {
        secondDerivativeName = d2tName;
    } else {
        secondDerivativeName = quantityName + " 2nd derivative";
    }
}

QuantityMetadata getMetadata(const QuantityId key) {
    switch (key) {
    case QuantityId::POSITION:
        return QuantityMetadata("Position", L"r", ValueEnum::VECTOR, "Velocity", "Acceleration");
    case QuantityId::MASS:
        return QuantityMetadata("Particle mass", L"m", ValueEnum::SCALAR);
    case QuantityId::PRESSURE:
        return QuantityMetadata("Pressure", L"p", ValueEnum::SCALAR);
    case QuantityId::DENSITY:
        return QuantityMetadata("Density", L"\u03C1" /*rho*/, ValueEnum::SCALAR);
    case QuantityId::ENERGY:
        return QuantityMetadata("Specific energy", L"u", ValueEnum::SCALAR);
    case QuantityId::TEMPERATURE:
        return QuantityMetadata("Temperature", L"T", ValueEnum::SCALAR);
    case QuantityId::SOUND_SPEED:
        return QuantityMetadata("Sound speed", L"c_s", ValueEnum::SCALAR);
    case QuantityId::DEVIATORIC_STRESS:
        return QuantityMetadata("Deviatoric stress", L"S", ValueEnum::TRACELESS_TENSOR);
    case QuantityId::SPECIFIC_ENTROPY:
        return QuantityMetadata("Specific entropy", L"s", ValueEnum::SCALAR);
    case QuantityId::GENERALIZED_ENERGY:
        return QuantityMetadata("Generalized energy", L"Y", ValueEnum::SCALAR);
    case QuantityId::GENERALIZED_PRESSURE:
        return QuantityMetadata("Generalized pressure", L"p^alpha", ValueEnum::SCALAR);
    case QuantityId::DAMAGE:
        return QuantityMetadata("Damage", L"D", ValueEnum::SCALAR);
    case QuantityId::EPS_MIN:
        return QuantityMetadata("Activation strain", L"\u03B5" /*epsilon*/, ValueEnum::SCALAR);
    case QuantityId::M_ZERO:
        return QuantityMetadata("Weibull exponent of stretched distribution", L"m_0", ValueEnum::SCALAR);
    case QuantityId::EXPLICIT_GROWTH:
        return QuantityMetadata("Explicit crack growth", L"???", ValueEnum::SCALAR);
    case QuantityId::N_FLAWS:
        return QuantityMetadata("Number of flaws", L"N_flaws", ValueEnum::INDEX);
    case QuantityId::STRESS_REDUCING:
        return QuantityMetadata("Yielding reduce", L"Red", ValueEnum::SCALAR);
    case QuantityId::VELOCITY_GRADIENT:
        return QuantityMetadata("Velocity gradient", L"\u2207v" /*nabla v*/, ValueEnum::SYMMETRIC_TENSOR);
    case QuantityId::VELOCITY_DIVERGENCE:
        return QuantityMetadata("Velocity divergence", L"\u2207\u22C5v" /*nabla cdot v*/, ValueEnum::SCALAR);
    case QuantityId::VELOCITY_ROTATION:
        return QuantityMetadata("Velocity rotation", L"\u2207\u2A2Fv" /*nabla cross v*/, ValueEnum::VECTOR);
    case QuantityId::STRAIN_RATE_CORRECTION_TENSOR:
        return QuantityMetadata("Correction tensor", L"C", ValueEnum::SYMMETRIC_TENSOR);
    case QuantityId::VELOCITY_LAPLACIAN:
        return QuantityMetadata("Velocity laplacian", L"\u0394v" /*Delta v*/, ValueEnum::VECTOR);
    case QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE:
        return QuantityMetadata("Gradient of velocity divergence",
            L"\u2207(\u2207\u22C5v)" /*nabla(nabla cdot v)*/,
            ValueEnum::VECTOR);
    case QuantityId::FRICTION:
        return QuantityMetadata("Friction", L"f", ValueEnum::VECTOR);
    case QuantityId::ENERGY_LAPLACIAN:
        return QuantityMetadata("Energy laplacian", L"\u0394u" /*Delta u*/, ValueEnum::SCALAR);
    case QuantityId::AV_ALPHA:
        return QuantityMetadata("AV alpha", L"\u03B1_AV" /*alpha_AV*/, ValueEnum::SCALAR);
    case QuantityId::AV_STRESS:
        return QuantityMetadata("Artificial stress", L"R", ValueEnum::SYMMETRIC_TENSOR);
    case QuantityId::AV_BALSARA:
        return QuantityMetadata("Balsara switch", L"f", ValueEnum::SCALAR);
    case QuantityId::INTERPARTICLE_SPACING_KERNEL:
        return QuantityMetadata("Interparticle spacing kernel", L"w(\u0394 p)", ValueEnum::SCALAR);
    case QuantityId::DISPLACEMENT:
        return QuantityMetadata("Displacement", L"u", ValueEnum::VECTOR);
    case QuantityId::FLAG:
        return QuantityMetadata("Flag", L"flag", ValueEnum::INDEX);
    case QuantityId::MATERIAL_ID:
        return QuantityMetadata("Material ID", L"matID", ValueEnum::INDEX);
    case QuantityId::XSPH_VELOCITIES:
        return QuantityMetadata("XSPH correction", L"xsph", ValueEnum::VECTOR);
    case QuantityId::GRAD_H:
        return QuantityMetadata("Grad-h terms", L"\u03A9" /*Omega*/, ValueEnum::SCALAR);
    case QuantityId::AGGREGATE_ID:
        return QuantityMetadata("Aggregate ID", L"i", ValueEnum::INDEX);
    case QuantityId::ANGULAR_FREQUENCY:
        return QuantityMetadata("Angular frequency", L"\u03C9" /*omega*/, ValueEnum::VECTOR);
    case QuantityId::ANGULAR_MOMENTUM:
        return QuantityMetadata("Angular momentum", L"L", ValueEnum::VECTOR);
    case QuantityId::NEIGHBOUR_CNT:
        return QuantityMetadata("Neigh. cnt", L"N_neigh", ValueEnum::INDEX);
    case QuantityId::SURFACE_NORMAL:
        return QuantityMetadata("Surf. normal", L"n", ValueEnum::VECTOR);
    case QuantityId::MOMENT_OF_INERTIA:
        return QuantityMetadata("Mom. of intertia", L"I", ValueEnum::SCALAR);
    case QuantityId::PHASE_ANGLE:
        return QuantityMetadata("Phase angle", L"\u03C6" /*phi*/, ValueEnum::VECTOR);
    case QuantityId::SMOOTHING_LENGTH:
        return QuantityMetadata("Smoothing length", L"h", ValueEnum::SCALAR);
    case QuantityId::UVW:
        return QuantityMetadata("Mapping coordinates", L"uvw", ValueEnum::VECTOR);
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
