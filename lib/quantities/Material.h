#pragma once

#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Eos;
}

/// Settings and functions for given material.
/// \todo material per solver? How to avoid storing parameters the solver doesn't need?
struct Material : public Noncopyable {
    std::unique_ptr<Abstract::Eos> eos; /// equation of state for given material
    Float shearModulus = 0._f;
    Float youngModulus = 0._f;
    Float elasticityLimit = 0._f;
    Float adiabaticIndex = 0._f;

    Material();

    ~Material();

    Material(const BodySettings& settings);

    Material(Material&& other);

    Material& operator=(Material&& other);
};

NAMESPACE_SPH_END
