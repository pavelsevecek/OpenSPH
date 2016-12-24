#pragma once

#include "system/Settings.h"

/// Generating initial conditions


NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Solver;
    class Domain;
}

class InitialConditions : public Noncopyable {
private:
    std::unique_ptr<Abstract::Solver> solver;

public:
    InitialConditions(const GlobalSettings& globalSettings);

    /// Creates particles by filling given domain. Particles are created on positions given by distribution in
    /// bodySettings. Beside positions of particles, the function initialize particle masses, pressure and
    /// sound speed, assuming both the pressure and sound speed are computed from equation of state. All other
    /// quantities, including density and energy, must be initialized by solver->initialize
    /// \todo generalize for entropy solver
    /// \todo setting velocities and rotation
    void addBody(Abstract::Domain& domain, const GlobalSettings& globalSettings, BodySettings& bodySettings);
};

NAMESPACE_SPH_END
