#pragma once

/// Storage of all quantities of SPH particles.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "storage/Quantity.h"

NAMESPACE_SPH_BEGIN

/// Unique ID of basic quantities of SPH particles
/// Order MUST match order in QuantityMap
namespace QuantityKey {
    enum BasicKey {
        R   = 0, ///< Positions (velocities, accelerations) of particles
        M   = 1, ///< Paricles masses
        P   = 2, ///< Pressure
        RHO = 3, ///< Density
        U   = 4, ///< Specific internal energy
        CS  = 5, ///< Sound speed
        S   = 6, ///< Deviatoric stress tensor
        D   = 7, ///< Damage
    };
};

struct QuantityDescriptor {
    const int key;
    const ValueEnum valueEnum;
    const TemporalEnum temporalEnum;
};


/// Defines type of the value and number of derivatives for given key.
/// Can be extended by partial specialization.
template <int TKey>
struct QuantityMap {
    // this array needs to be inside a class, otherwise it produces internal compiler error ...
    // clang-format off
    static constexpr QuantityDescriptor quantityDescriptors[] =
        { { QuantityKey::R,     ValueEnum::VECTOR,              TemporalEnum::SECOND_ORDER },
          { QuantityKey::M,     ValueEnum::SCALAR,              TemporalEnum::ZERO_ORDER },
          { QuantityKey::P,     ValueEnum::SCALAR,              TemporalEnum::ZERO_ORDER},
          { QuantityKey::RHO,   ValueEnum::SCALAR,              TemporalEnum::FIRST_ORDER },
          { QuantityKey::U,     ValueEnum::SCALAR,              TemporalEnum::FIRST_ORDER},
          { QuantityKey::CS,    ValueEnum::SCALAR,              TemporalEnum::ZERO_ORDER },
          { QuantityKey::S,     ValueEnum::TRACELESS_TENSOR,    TemporalEnum::FIRST_ORDER },
          { QuantityKey::D,     ValueEnum::SCALAR,              TemporalEnum::FIRST_ORDER } };
    // clang-format on

    static constexpr QuantityDescriptor descriptor = quantityDescriptors[int(TKey)];
    using Type                                     = typename GetTypeFromValue<descriptor.valueEnum>::Type;
    static constexpr TemporalEnum temporalEnum     = descriptor.temporalEnum;
};

/// alias
template <int TKey>
using QuantityType = typename QuantityMap<TKey>::Type;

/// \todo rewrite this to something less stupid
INLINE std::string getQuantityName(const int key) {
    switch (key) {
    case QuantityKey::R:
        return "Position";
    case QuantityKey::M:
        return "Particle mass";
    case QuantityKey::P:
        return "Pressure";
    case QuantityKey::RHO:
        return "Density";
    case QuantityKey::U:
        return "Specific energy";
    case QuantityKey::CS:
        return "Sound speed";
    case QuantityKey::S:
        return "Deviatoric stress tensor";
    case QuantityKey::D:
        return "Damage";
    default:
        NOT_IMPLEMENTED;
    }
}

INLINE std::string getDerivativeName(const int key) {
    switch (key) {
    case QuantityKey::R:
        return "Velocity";
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
