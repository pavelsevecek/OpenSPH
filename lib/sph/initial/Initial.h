#pragma once

#include "common/ForwardDecl.h"
#include "geometry/Vector.h"
#include "objects/containers/ArrayView.h"
#include <memory>

/// Generating initial conditions


NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Rng;
}

/// \todo possibly generalize, allowing to create generic context as needed by components of the run
struct InitialContext {
    std::unique_ptr<Abstract::Rng> rng;
};

/// All particles created in one run should be created using the same InitialConditions object. If multiple
/// objects are used, quantity FLAG must be manually updated to be unique for each body in the simulation.
class InitialConditions : public Noncopyable {
private:
    Storage& storage;
    std::unique_ptr<Abstract::Solver> solver;
    Size bodyIndex = 0;

    InitialContext context;

public:
    InitialConditions(Storage& storage, const RunSettings& settings);

    InitialConditions(Storage& storage,
        std::unique_ptr<Abstract::Solver>&& solver,
        const RunSettings& settings);

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

    /// Overload with custom material.
    void addBody(const Abstract::Domain& domain,
        std::unique_ptr<Abstract::Material>&& material,
        const Vector& velocity = Vector(0._f),
        const Vector& angularVelocity = Vector(0._f));

    struct Body {
        std::unique_ptr<Abstract::Domain> domain;
        std::unique_ptr<Abstract::Material> material;
        Vector velocity;
        Vector angularVelocity;

        Body();

        Body(std::unique_ptr<Abstract::Domain>&& domain,
            std::unique_ptr<Abstract::Material>&& material,
            const Vector& velocity = Vector(0._f),
            const Vector& angularVelocity = Vector(0._f));

        Body(std::unique_ptr<Abstract::Domain>&& domain,
            const BodySettings& settings,
            const Vector& velocity = Vector(0._f),
            const Vector& angularVelocity = Vector(0._f));

        Body(Body&& other);

        ~Body();
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
    void addHeterogeneousBody(Body&& environment, ArrayView<Body> bodies);

private:
    void setQuantities(Storage& storage,
        Abstract::Material& material,
        const Float volume,
        const Vector& velocity,
        const Vector& angularVelocity);
};

NAMESPACE_SPH_END
