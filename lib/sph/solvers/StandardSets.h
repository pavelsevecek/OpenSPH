#pragma once

/// \file StandardSets.h
/// \brief Standard SPH formulation that uses continuity equation for solution of density
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "sph/equations/EquationTerm.h"

NAMESPACE_SPH_BEGIN

/// \brief Standard SPH equation set, using density and specific energy as independent variables.
///
/// Evolves density using continuity equation and energy using energy equation. Works with any artificial
/// viscosity and any equation of state.
EquationHolder getStandardEquations(const RunSettings& settings, const EquationHolder& other = {});

NAMESPACE_SPH_END
