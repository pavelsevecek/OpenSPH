#include "quantities/QuantityIds.h"
#include "common/Assert.h"

NAMESPACE_SPH_BEGIN

std::string getQuantityName(const QuantityId key) {
    switch (key) {
    case QuantityId::POSITIONS:
        return "Position";
    case QuantityId::MASSES:
        return "Particle mass";
    case QuantityId::PRESSURE:
        return "Pressure";
    case QuantityId::DENSITY:
        return "Density";
    case QuantityId::ENERGY:
        return "Spec. energy";
    case QuantityId::SOUND_SPEED:
        return "Sound speed";
    case QuantityId::DEVIATORIC_STRESS:
        return "Stress";
    case QuantityId::SPECIFIC_ENTROPY:
        return "Spec. entropy";
    case QuantityId::ENERGY_DENSITY:
        return "Energy density";
    case QuantityId::ENERGY_PER_PARTICLE:
        return "Energy per particle";
    case QuantityId::DAMAGE:
        return "Damage";
    case QuantityId::EPS_MIN:
        return "Activation strain";
    case QuantityId::M_ZERO:
        return "Weibull m_0";
    case QuantityId::EXPLICIT_GROWTH:
        return "Explicit crack growth";
    case QuantityId::N_FLAWS:
        return "Number of flaws";
    case QuantityId::FLAW_ACTIVATION_IDX:
        return "Flaw activation idx";
    case QuantityId::YIELDING_REDUCE:
        return "Yielding reduce";
    case QuantityId::VELOCITY_GRADIENT:
        return "grad v";
    case QuantityId::VELOCITY_DIVERGENCE:
        return "div v";
    case QuantityId::VELOCITY_ROTATION:
        return "rot v";
    case QuantityId::STRENGTH_VELOCITY_GRADIENT:
        return "Strength grad v";
    case QuantityId::ANGULAR_MOMENTUM_CORRECTION:
        return "Correction tensor C_ij";
    case QuantityId::AV_ALPHA:
        return "alpha_AV";
    case QuantityId::AV_BETA:
        return "beta_AV";
    case QuantityId::NEIGHBOUR_CNT:
        return "Neigh. cnt";
    case QuantityId::MATERIAL_IDX:
        return "Mat. idx";
    case QuantityId::FLAG:
        return "Flag";
    default:
        NOT_IMPLEMENTED;
    }
}

std::string getDerivativeName(const QuantityId key) {
    switch (key) {
    case QuantityId::POSITIONS:
        return "Velocity";
    case QuantityId::DEVIATORIC_STRESS:
        return "[debug] dS/dt";
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
