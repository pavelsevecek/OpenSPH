#pragma once

#include "system/Settings.h"
#include <memory>

/// Generating initial conditions


NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Solver;
    class Domain;
}
class Storage;

class InitialConditions : public Noncopyable {
private:
    std::shared_ptr<Storage> storage;
    std::unique_ptr<Abstract::Solver> solver;

public:
    InitialConditions(const std::shared_ptr<Storage> storage, const GlobalSettings& globalSettings);

    ~InitialConditions();

    /// Creates particles by filling given domain. Particles are created on positions given by distribution in
    /// bodySettings. Beside positions of particles, the function initialize particle masses, pressure and
    /// sound speed, assuming both the pressure and sound speed are computed from equation of state. All other
    /// quantities, including density and energy, must be initialized by solver->initialize
    /// \todo generalize for entropy solver
    /// \todo setting velocities and rotation
    void addBody(Abstract::Domain& domain, BodySettings& bodySettings);
};

NAMESPACE_SPH_END
