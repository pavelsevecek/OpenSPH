#pragma once

#include "solvers/AbstractSolver.h"

NAMESPACE_SPH_BEGIN

/// Uses solver of Saitoh & Makino (2013). Instead of density and specific energy, independent variables are
/// energy density (q) and internal energy of i-th particle (U). Otherwise, the solver is similar to
/// SummationSolver; the energy density is computed using direct summation by self-consistent solution with
/// smoothing length. Works only for ideal gas EoS!
class DensityIndependentSolver : public Abstract::Solver {};

NAMESPACE_SPH_END
