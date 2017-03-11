#pragma once

#include "geometry/Vector.h"
#include "objects/ForwardDecl.h"
#include "objects/containers/ArrayView.h"
#include <memory>

/// Generating initial conditions


NAMESPACE_SPH_BEGIN

/// All particles created in one run should be created using the same InitialConditions object. If multiple
/// objects are used, quantity FLAG must be manually updated to be unique for each body in the simulation.
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

    struct Body {
        const Abstract::Domain& domain;
        BodySettings& settings;
        Vector velocity;
        Vector angularVelocity;
    };

    /// Creates particles composed of different materials.
    /// \param environement Base body, domain of which defines the body. No particles are generated outside of
    ///                     this domain. By default, all particles have the material and velocity given by
    ///                     this
    ///                     body.
    /// \param bodies List of bodies created inside the main environemnt. Each can have different material and
    ///               have different initial velocity. These bodies don't add more particles (particle count
    ///               in
    ///               settings is irrelevant), they simply override particles created by environment body.
    ///               If multiple bodies overlap, particles are assigned to body listed first in the array.
    void addHeterogeneousBody(const Body& environment, ArrayView<const Body> bodies);

private:
    void setQuantities(Storage& storage,
        const BodySettings& settings,
        const Float volume,
        const Vector& velocity,
        const Vector& angularVelocity);
};

NAMESPACE_SPH_END
