#include "quantities/QuantityIds.h"
#include "common/Assert.h"

NAMESPACE_SPH_BEGIN

QuantityMetadata::QuantityMetadata(const std::string& fullName,
    const std::string& label,
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
    case QuantityId::POSITIONS:
        return QuantityMetadata("Position", "r", ValueEnum::VECTOR, "Velocity", "Acceleration");
    case QuantityId::MASSES:
        return QuantityMetadata("Particle mass", "m", ValueEnum::SCALAR);
    case QuantityId::PRESSURE:
        return QuantityMetadata("Pressure", "p", ValueEnum::SCALAR);
    case QuantityId::DENSITY:
        return QuantityMetadata("Density", "rho", ValueEnum::SCALAR);
    case QuantityId::ENERGY:
        return QuantityMetadata("Specific energy", "u", ValueEnum::SCALAR);
    case QuantityId::SOUND_SPEED:
        return QuantityMetadata("Sound speed", "c_s", ValueEnum::SCALAR);
    case QuantityId::DEVIATORIC_STRESS:
        return QuantityMetadata("Deviatoric stress", "s", ValueEnum::TRACELESS_TENSOR);
    case QuantityId::SPECIFIC_ENTROPY:
        return QuantityMetadata("Specific entropy", "s", ValueEnum::SCALAR);
    case QuantityId::ENERGY_DENSITY:
        return QuantityMetadata("Energy density", "q", ValueEnum::SCALAR);
    case QuantityId::ENERGY_PER_PARTICLE:
        return QuantityMetadata("Energy per particle", "U", ValueEnum::SCALAR);
    case QuantityId::DAMAGE:
        return QuantityMetadata("Damage", "D", ValueEnum::SCALAR);
    case QuantityId::EPS_MIN:
        return QuantityMetadata("Activation strain", "eps", ValueEnum::SCALAR);
    case QuantityId::M_ZERO:
        return QuantityMetadata("Weibull exponent of stretched distribution", "m_0", ValueEnum::SCALAR);
    case QuantityId::EXPLICIT_GROWTH:
        return QuantityMetadata("Explicit crack growth", "???", ValueEnum::SCALAR);
    case QuantityId::N_FLAWS:
        return QuantityMetadata("Number of flaws", "N_flaws", ValueEnum::INDEX);
    case QuantityId::FLAW_ACTIVATION_IDX:
        return QuantityMetadata("Flaw activation idx", "Act", ValueEnum::INDEX);
    case QuantityId::STRESS_REDUCING:
        return QuantityMetadata("Yielding reduce", "Red", ValueEnum::SCALAR);
    case QuantityId::VELOCITY_GRADIENT:
        return QuantityMetadata("Velocity gradient", "grad v", ValueEnum::SYMMETRIC_TENSOR);
    case QuantityId::VELOCITY_DIVERGENCE:
        return QuantityMetadata("Velocity divergence", "div v", ValueEnum::SCALAR);
    case QuantityId::VELOCITY_ROTATION:
        return QuantityMetadata("Velocity rotation", "rot v", ValueEnum::VECTOR);
    case QuantityId::STRENGTH_VELOCITY_GRADIENT:
        return QuantityMetadata("Strength velocity gradient", "grad_S v", ValueEnum::SYMMETRIC_TENSOR);
    case QuantityId::ANGULAR_MOMENTUM_CORRECTION:
        return QuantityMetadata("Correction tensor", "C_ij", ValueEnum::SYMMETRIC_TENSOR);
    case QuantityId::AV_ALPHA:
        return QuantityMetadata("AV alpha", "alpha_AV", ValueEnum::SCALAR);
    case QuantityId::AV_BETA:
        return QuantityMetadata("AV beta", "beta_AV", ValueEnum::SCALAR);
    case QuantityId::AV_STRESS:
        return QuantityMetadata("Artificial stress", "R", ValueEnum::SYMMETRIC_TENSOR);
    case QuantityId::INTERPARTICLE_SPACING_KERNEL:
        return QuantityMetadata("Interparticle spacing kernel", "w(Delta p)", ValueEnum::SCALAR);
    case QuantityId::DISPLACEMENT:
        return QuantityMetadata("Displacement", "u", ValueEnum::VECTOR);
    case QuantityId::FLAG:
        return QuantityMetadata("Flag", "flag", ValueEnum::INDEX);
    case QuantityId::MATERIAL_ID:
        return QuantityMetadata("MaterialId", "matID", ValueEnum::INDEX);
    case QuantityId::XSPH_VELOCITIES:
        return QuantityMetadata("XSPH correction", "v_xsph", ValueEnum::VECTOR);
    case QuantityId::GRAD_H:
        return QuantityMetadata("Grad-h terms", "Omega", ValueEnum::SCALAR);
    case QuantityId::GRAVITY_POTENTIAL:
        return QuantityMetadata("Grav. potential", "Phi", ValueEnum::SCALAR);
    case QuantityId::ANGULAR_VELOCITY:
        return QuantityMetadata("Angular velocity", "Omega", ValueEnum::VECTOR);
    case QuantityId::NEIGHBOUR_CNT:
        return QuantityMetadata("Neigh. cnt", "N_neigh", ValueEnum::INDEX);
    case QuantityId::SURFACE_NORMAL:
        return QuantityMetadata("Surf. normal", "n", ValueEnum::VECTOR);
    case QuantityId::MOMENT_OF_INERTIA:
        return QuantityMetadata("Mom. of intertia", "I", ValueEnum::SCALAR);
    case QuantityId::PHASE_ANGLE:
        return QuantityMetadata("Phase angle", "phi", ValueEnum::VECTOR);
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
