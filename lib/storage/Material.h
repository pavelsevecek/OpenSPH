#pragma once

/// \todo move include to cpp
#include "physics/Eos.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN


/// Settings and functions for given material.
/// \todo material per solver? How to avoid storing parameters the solver doesn't need?
struct Material : public Noncopyable {
    std::unique_ptr<Abstract::Eos> eos; /// equation of state for given material
    Float shearModulus = 0._f;
    Float elasticityLimit = 0._f;

    Material() = default;

    Material(const BodySettings& settings) {
        eos = Factory::getEos(settings);
        shearModulus = settings.get<Float>(BodySettingsIds::SHEAR_MODULUS);
        elasticityLimit = settings.get<Float>(BodySettingsIds::VON_MISES_ELASTICITY_LIMIT);
    }

    Material(Material&& other)
        : eos(std::move(other.eos))
        , shearModulus(other.shearModulus)
        , elasticityLimit(other.elasticityLimit) {}

    Material& operator=(Material&& other) {
        eos = std::move(other.eos);
        shearModulus = other.shearModulus;
        elasticityLimit = other.elasticityLimit;
        return *this;
    }
};

NAMESPACE_SPH_END
