#pragma once

NAMESPACE_SPH_BEGIN

/// Variation of Summation solver, evolving specific entropy (s) rather than specific energy (u), see
/// Springel & Hernquist (2002). The advantage of the method is that in the absense of dissipation, there is
/// no need to evolve the entropy in time and we need to solve one equation less. Main disadvantage is that
/// the method only works for ideal gas EoS, as other EoS cannot be written (or it is difficult to write them)
/// in form P(rho, s).
/// Generally does not converse energy!

class EntropySolver : public SolverBase {
}

NAMESPACE_SPH_END
