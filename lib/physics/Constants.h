#pragma once

/// \file Constants.h
/// \brief Definitions of physical constaints (in SI units).
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/Globals.h"

NAMESPACE_SPH_BEGIN

namespace Constants {
    const Float gasConstant = 8.3144598_f; // J mol^−1 K^−1

    const Float atomicMass = 1.660539040e-27_f; // kg

    /// Boltzmann constant
    const Float boltzmann = 1.380648e-23_f; // J K^-1

    /// Gravitational constant (CODATA 2014)
    const Float gravity = 6.67408e-11_f; // m^3 kg^-1 s^-2

    /// Astronomical unit (exactly)
    const Float au = 149597870700._f; // m

    /// Solar mass
    /// http://asa.usno.navy.mil/static/files/2014/Astronomical_Constants_2014.pdf
    const Float M_sun = 1.9884e30_f; // kg

    /// Earth mass
    const Float M_earth = 5.9722e24_f; // kg
} // namespace Constants

NAMESPACE_SPH_END
