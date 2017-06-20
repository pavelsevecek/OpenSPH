#pragma once

/// \file Initial.h
/// \brief Generating initial conditions of SPH particles
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/ForwardDecl.h"
#include "geometry/Vector.h"
#include "objects/containers/ArrayView.h"
#include "objects/wrappers/AutoPtr.h"
#include "quantities/AbstractMaterial.h"

NAMESPACE_SPH_BEGIN

/// \brief Non-owning view of particles belonging to the same body
///
/// Object allows to access, modify and setup additional properties of the particles created by \ref
/// InitialConditions.
class Body {};

/// \brief Object for adding one or more bodies with given material into Storage
///
/// The object is intended to only be constructed on stack, set up needed bodies and get destroyed when going
/// out of scope. As it holds a reference to the storage, it is unsafe to store InitialConditions and create
/// additional bodies during the run. All particles created in one run should be created using the same
/// InitialConditions object. If multiple objects are used, quantity QuantityId::FLAG must be manually updated
/// to be unique for each body in the simulation.
///
/// The object possibly changes the storage from destructor, after all bodies have been added. If for whatever
/// reason we wish to prevent this, the initial condition setup can be explicitly ended by calling \ref
/// finalize function. After that, no more bodies can be added into the storage.
class InitialConditions : public Noncopyable {
private:
    /// Reference to the storage where all the bodies are saved
    Storage& storage;

    /// Solver used for creating necessary quantities. Does not necessarily have to be the same the solver
    /// used for the actual run, although it is recommended to make sure all the quantities are set up
    /// correctly.
    AutoPtr<Abstract::Solver> solver;

    /// Counter incremented every time a body is added, used for setting up FLAG quantity
    Size bodyIndex = 0;

    /// Shared data when creating bodies
    MaterialInitialContext context;

    /// Params used after all bodies are created
    struct {
        bool storeInitialPositions = false;
    } finalization;


public:
    /// Constructs object by taking a reference to particle storage. Subsequent calls of \ref addBody function
    /// fill this storage with particles.
    /// \param storage Particle storage, must exist at least as long as this object.
    /// \param solver Solver used to create all the necessary quantities. Also must exist for the duration of
    ///               this object as it is stored by reference.
    /// \param settings Run settings
    InitialConditions(Storage& storage, Abstract::Solver& solver, const RunSettings& settings);

    /// Constructor creating solver from values in settings.
    InitialConditions(Storage& storage, const RunSettings& settings);

    ~InitialConditions();

    /// \brief Creates particles by filling given domain.
    ///
    /// Particles are created on positions given by distribution in bodySettings. Beside positions of
    /// particles, the function initialize particle masses, pressure and sound speed, assuming both the
    /// pressure and sound speed are computed from equation of state. The function also calls
    /// Abstract::Solver::create to initialze quantities needed by used solver, either a solver given in
    /// constructor or a default one based on RunSettings parameters.
    /// \param domain Spatial domain where the particles are placed. The domain should not overlap a body
    ///               already added into the storage as that would lead to incorrect density estimating in
    ///               overlapping regions.
    /// \param bodySettings Parameters of the body
    /// \todo generalize for entropy solver
    void addBody(const Abstract::Domain& domain,
        const BodySettings& bodySettings,
        const Vector& velocity = Vector(0._f),
        const Vector& angularVelocity = Vector(0._f));

    /// \copydoc addBody
    /// Overload with custom material.
    void addBody(const Abstract::Domain& domain,
        AutoPtr<Abstract::Material>&& material,
        const Vector& velocity = Vector(0._f),
        const Vector& angularVelocity = Vector(0._f));

    struct Body {
        AutoPtr<Abstract::Domain> domain;
        AutoPtr<Abstract::Material> material;
        Vector velocity;
        Vector angularVelocity;

        Body();

        Body(AutoPtr<Abstract::Domain>&& domain,
            AutoPtr<Abstract::Material>&& material,
            const Vector& velocity = Vector(0._f),
            const Vector& angularVelocity = Vector(0._f));

        Body(AutoPtr<Abstract::Domain>&& domain,
            const BodySettings& settings,
            const Vector& velocity = Vector(0._f),
            const Vector& angularVelocity = Vector(0._f));

        Body(Body&& other);

        ~Body();
    };

    /// Creates particles composed of different materials.
    /// \param environment Base body, domain of which defines the body. No particles are generated outside of
    ///                    this domain. By default, all particles have the material and velocity given by
    ///                    this body.
    /// \param bodies List of bodies created inside the main environemnt. Each can have different material and
    ///               have different initial velocity. These bodies don't add more particles (particle count
    ///               in settings is irrelevant), they simply override particles created by environment body.
    ///               If multiple bodies overlap, particles are assigned to body listed first in the array.
    void addHeterogeneousBody(Body&& environment, ArrayView<Body> bodies);

    /// Ends the initial condition settings. Storage is then no longer used by the object. Does not have to be
    /// called manually.
    void finalize();

private:
    void createCommon(const RunSettings& settings);

    void setQuantities(Storage& storage,
        Abstract::Material& material,
        const Float volume,
        const Vector& velocity,
        const Vector& angularVelocity);
};

NAMESPACE_SPH_END
