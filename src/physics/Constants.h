#pragma once

/// Definitions of physical constaints (in SI units).
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Vector.h"

NAMESPACE_SPH_BEGIN

namespace Constants {
    const Float gasConstant = 8.3144598_f; // J mol^−1 K^−1

    const Float atomicMass = 1.660539040e-27_f; // kg

    const Float boltzmann = 1.380648e-23_f; // J K^-1
}

NAMESPACE_SPH_END
