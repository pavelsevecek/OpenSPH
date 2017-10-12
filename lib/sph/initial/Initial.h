#pragma once

/// \file Initial.h
/// \brief Generating initial conditions of SPH particles
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/ForwardDecl.h"
#include "objects/containers/ArrayView.h"
#include "objects/geometry/Vector.h"
#include "objects/wrappers/AutoPtr.h"
#include "quantities/IMaterial.h"

NAMESPACE_SPH_BEGIN

/// \brief Non-owning view of particles belonging to the same body
///
/// Object allows to access, modify and setup additional properties of the particles created by \ref
/// InitialConditions.
class BodyView {
private:
    /// Reference to the storage.
    Storage& storage;

    /// Index of this body.
    Size bodyIndex;

public:
    BodyView(Storage& storage, const Size bodyIndex);

    /// Adds a velocity vector to all particles of the body. If the particles previously had nonzero
    /// velocities, the velocities are added; the previous velocities are not erased.
    /// \param v Velocity vector added to all particles.
    /// \returns Reference to itself.
    BodyView& addVelocity(const Vector& v);

    /// Predefined types of center point
    enum class RotationOrigin {
        FRAME_ORIGIN,   ///< Add angular velocity with respect to origin of coordinates
        CENTER_OF_MASS, ///< Rotate the body around its center of mass
    };

    /// Adds an angular velocity to all particles of the body. The new velocities are added to velocities
    /// previously assigned to the particles.
    /// \param omega Angular velocity (in radians/s), the direction of the vector is the axis of rotation.
    /// \param origin Center point of the rotation, see RotationOrigin enum.
    /// \returns Reference to itself.
    BodyView& addRotation(const Vector& omega, const RotationOrigin origin);

    /// Adds an angular velocity to all particles of the body. The new velocities are added to velocities
    /// previously assigned to the particles.
    /// \param omega Angular velocity (in radians/s), the direction of the vector is the axis of rotation.
    /// \param origin Vector defining the center point of the rotation.
    /// \returns Reference to itself.
    BodyView& addRotation(const Vector& omega, const Vector& origin);

private:
    Vector getOrigin(const RotationOrigin origin) const;
};

/// \brief Object for adding one or more bodies with given material into Storage
///
/// All particles created in one run should be created using the same InitialConditions object. If multiple
/// objects are used, quantity QuantityId::FLAG must be manually updated to be unique for each body in the
/// simulation.
class InitialConditions : public Noncopyable {
private:
    /// Solver used for creating necessary quantities. Does not necessarily have to be the same the solver
    /// used for the actual run, although it is recommended to make sure all the quantities are set up
    /// correctly.
    AutoPtr<ISolver> solver;

    /// Counter incremented every time a body is added, used for setting up FLAG quantity
    Size bodyIndex = 0;

    /// Shared data when creating bodies
    MaterialInitialContext context;

public:
    /// \brief Constructs object by taking a reference to particle storage.
    ///
    /// Subsequent calls of \ref addBody function fill this storage with particles.
    /// \param solver Solver used to create all the necessary quantities. Also must exist for the duration
    ///               of this object as it is stored by reference.
    /// \param settings Run settings
    InitialConditions(ISolver& solver, const RunSettings& settings);

    /// \brief Constructor creating solver from values in settings.
    ///
    /// \attention When using this overload, the solver used for the run should also be created from settings
    /// (which is done by default). Using different solver may result in incorrect initialization of
    /// quantities. Mostly, this will throw an exception or assert, but in case the custom solver uses the
    /// same quantities as the default one, but it initializes them to different values, this error would go
    /// unnoticed.
    InitialConditions(const RunSettings& settings);

    ~InitialConditions();

    /// \brief Creates particles by filling given domain.
    ///
    /// Particles are created on positions given by distribution in bodySettings. Beside positions of
    /// particles, the function initialize particle masses, pressure and sound speed, assuming both the
    /// pressure and sound speed are computed from equation of state. The function also calls
    /// \ref ISolver::create to initialze quantities needed by used solver, either a solver given in
    /// constructor or a default one based on RunSettings parameters.
    /// \param storage Particle storage to which the new body is added
    /// \param domain Spatial domain where the particles are placed. The domain should not overlap a body
    ///               already added into the storage as that would lead to incorrect density estimating in
    ///               overlapping regions.
    /// \param bodySettings Parameters of the body
    /// \todo generalize for entropy solver
    BodyView addBody(Storage& storage, const IDomain& domain, const BodySettings& bodySettings);

    /// Adds a body by explicitly specifying its material.
    /// \copydoc addBody
    BodyView addBody(Storage& storage, const IDomain& domain, AutoPtr<IMaterial>&& material);


    /// Holds data needed to create a single body in \ref addHeterogeneousBody function.
    struct BodySetup {
        AutoPtr<IDomain> domain;
        AutoPtr<IMaterial> material;

        /// Creates a body with undefined domain and material
        BodySetup();

        /// Creates a body by specifying its domain and material
        BodySetup(AutoPtr<IDomain>&& domain, AutoPtr<IMaterial>&& material);

        /// Creates a body by specifying its domain; material is created from parameters in settings
        BodySetup(AutoPtr<IDomain>&& domain, const BodySettings& settings);

        /// Move constructor
        BodySetup(BodySetup&& other);

        ~BodySetup();
    };

    /// Creates particles composed of different materials.
    /// \param storage Particle storage to which the new body is added
    /// \param environment Base body, domain of which defines the body. No particles are generated outside
    ///                    of this domain. By default, all particles have the material given by this body.
    /// \param bodies List of bodies created inside the main environemnt. Each can have different material
    ///               and have different initial velocity. These bodies don't add more particles (particle
    ///               count in settings is irrelevant), they simply override particles created by environment
    ///               body. If multiple bodies overlap, particles are assigned to body listed first in the
    ///               array.
    /// \return Array of n+1 BodyViews, where n is the size of \ref bodies parameter. The first one
    ///         corresponds to the environment, the rest are the bodies inside the environment in the
    ///         order they were passed in \ref bodies.
    Array<BodyView> addHeterogeneousBody(Storage& storage,
        BodySetup&& environment,
        ArrayView<BodySetup> bodies);

    /// \brief Ends the initial condition settings.
    ///
    /// Storage is then no longer used by the object. Does not have to be called manually, it is called from
    /// destructor.
    void finalize();

private:
    void createCommon(const RunSettings& settings);

    void setQuantities(Storage& storage, IMaterial& material, const Float volume);
};

NAMESPACE_SPH_END
