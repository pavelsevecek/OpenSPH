#pragma once

#include "objects/containers/ArrayView.h"
#include "objects/ForwardDecl.h"
#include <memory>

/// Generating initial conditions


NAMESPACE_SPH_BEGIN


class InitialConditions : public Noncopyable {
private:
    std::shared_ptr<Storage> storage;
    std::unique_ptr<Abstract::Solver> solver;
    Size bodyIndex = 0;

public:
    InitialConditions(const std::shared_ptr<Storage> storage, const GlobalSettings& globalSettings);

    ~InitialConditions();

    /// Creates particles by filling given domain. Particles are created on positions given by distribution in
    /// bodySettings. Beside positions of particles, the function initialize particle masses, pressure and
    /// sound speed, assuming both the pressure and sound speed are computed from equation of state. All other
    /// quantities, including density and energy, must be initialized by solver->initialize
    /// \todo generalize for entropy solver
    void addBody(const Abstract::Domain& domain,
        const BodySettings& bodySettings,
        const Vector& velocity = Vector(0._f),
        const Vector& angularVelocity = Vector(0._f));

    /// Creates particles composed of different materials.
    /// \param assigner Function returning index of material for given position.
    /// \param settings Array of settings referenced by assigner.
    void addHeterogeneousBody(const Abstract::Domain& domain,
        const std::function<int(const Vector&)>& assigner,
        ArrayView<const BodySettings> settings,
        const Vector& velocity = Vector(0._f),
        const Vector& angularVelocity = Vector(0._f));
};

NAMESPACE_SPH_END
