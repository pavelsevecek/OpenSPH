#pragma once

/// \file Constants.h
/// \brief Definitions of physical constaints (in SI units).
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/Globals.h"

NAMESPACE_SPH_BEGIN

namespace Constants {

constexpr Float gasConstant = 8.3144598_f; // J mol^−1 K^−1

/// Atomic mass constant
constexpr Float atomicMass = 1.660539040e-27_f; // kg

/// Boltzmann constant
constexpr Float boltzmann = 1.380648e-23_f; // J K^-1

/// Stefan-Boltzmann constant
constexpr Float stefanBoltzmann = 5.670367e-8_f; // W m^2 K^-4

/// Planck constant
constexpr Float planckConstant = 6.62607015e-34_f; // Js

/// Gravitational constant (CODATA 2014)
constexpr Float gravity = 6.67408e-11_f; // m^3 kg^-1 s^-2

/// Speed of light in vacuum (exactly)
constexpr Float speedOfLight = 299792458._f; // ms^-1

/// Astronomical unit (exactly)
constexpr Float au = 149597870700._f; // m

/// Parsec
constexpr Float pc = 3.0856776e16_f; // m

/// Number of seconds in a year
constexpr Float year = 3.154e7_f; // s

/// Solar mass
/// http://asa.usno.navy.mil/static/files/2014/Astronomical_Constants_2014.pdf
constexpr Float M_sun = 1.9884e30_f; // kg

/// Earth mass
constexpr Float M_earth = 5.9722e24_f; // kg

/// Solar radius
constexpr Float R_sun = 6.957e8_f; // m

/// Earth radius
constexpr Float R_earth = 6.3781e6_f; // m

} // namespace Constants

NAMESPACE_SPH_END
