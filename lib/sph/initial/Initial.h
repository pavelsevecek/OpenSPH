#pragma once

/// \file Initial.h
/// \brief Generating initial conditions of SPH particles
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

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

    /// \brief Moves the particles of the body in given direction.
    ///
    /// The particles are moved relative to the current position.
    /// \param dr Displacement vector.
    /// \return Reference to itself.
    BodyView& displace(const Vector& dr);

    /// \brief Adds a velocity vector to all particles of the body.
    ///
    /// If the particles previously had nonzero velocities, the velocities are added; the previous velocities
    /// are not erased.
    /// \param v Velocity vector added to all particles.
    /// \returns Reference to itself.
    BodyView& addVelocity(const Vector& v);

    /// Predefined types of center point
    enum class RotationOrigin {
        FRAME_ORIGIN,   ///< Add angular velocity with respect to origin of coordinates
        CENTER_OF_MASS, ///< Rotate the body around its center of mass
    };

    /// \brief Adds an angular velocity to all particles of the body.
    ///
    /// The new velocities are added to velocities previously assigned to the particles.
    /// \param omega Angular velocity (in radians/s), the direction of the vector is the axis of rotation.
    /// \param origin Center point of the rotation, see RotationOrigin enum.
    /// \returns Reference to itself.
    BodyView& addRotation(const Vector& omega, const RotationOrigin origin);

    /// \brief Adds an angular velocity to all particles of the body.
    ///
    /// The new velocities are added to velocities previously assigned to the particles.
    /// \param omega Angular velocity (in radians/s), the direction of the vector is the axis of rotation.
    /// \param origin Vector defining the center point of the rotation.
    /// \returns Reference to itself.
    BodyView& addRotation(const Vector& omega, const Vector& origin);

private:
    Vector getOrigin(const RotationOrigin origin) const;
};

/// \brief Holds the information about a power-law size-frequency distributions.
struct PowerLawSfd {
    /// Exponent alpha of the power-law x^-alpha.
    ///
    /// Can be lower than 1 or negative, meaning there is more larger modies than smaller bodies. Cannot be
    /// exactly 1.
    Float exponent;

    /// Minimal and maximal value of the SFD
    Interval interval;

    /// \brief Generates a new value of the SFD by transforming given value from interval [0, 1].
    ///
    /// For x=0 and x=1, interval.lower() and interval.upper() is returned, respectively. Input value must lie
    /// in unit interval, checked by assert.
    INLINE Float operator()(const Float x) const {
        ASSERT(x >= 0._f && x <= 1._f);
        ASSERT(exponent != 1._f);
        const Float Rmin = pow(interval.lower(), 1._f - exponent);
        const Float Rmax = pow(interval.upper(), 1._f - exponent);
        const Float R = pow((Rmax - Rmin) * x + Rmin, 1._f / (1._f - exponent));
        ASSERT(R >= interval.lower() && R <= interval.upper(), R);
        return R;
    }
};


/// \brief Object for adding one or more bodies with given material into Storage
///
/// All particles created in one run should be created using the same InitialConditions object. If multiple
/// objects are used, quantity QuantityId::FLAG must be manually updated to be unique for each body in the
/// simulation.
class InitialConditions : public Noncopyable {
private:
    /// Scheduler that can be used to parallelize the generation of the initial conditions.
    IScheduler& scheduler;

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
    /// \param scheduler Scheduler used for parallelization.
    /// \param solver Solver used to create all the necessary quantities. Also must exist for the duration
    ///               of this object as it is stored by reference.
    /// \param settings Run settings
    InitialConditions(IScheduler& scheduler, ISolver& solver, const RunSettings& settings);

    /// \brief Constructor creating solver from values in settings.
    ///
    /// \attention When using this overload, the solver used for the run should also be created from settings
    /// (which is done by default). Using different solver may result in incorrect initialization of
    /// quantities. Mostly, this will throw an exception or assert, but in case the custom solver uses the
    /// same quantities as the default one, but it initializes them to different values, this error would go
    /// unnoticed.
    InitialConditions(IScheduler& scheduler, const RunSettings& settings);

    ~InitialConditions();

    /// \brief Creates a monolithic body by filling given domain with particles.
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
    /// \param body Parameters of the body
    /// \todo generalize for entropy solver
    BodyView addMonolithicBody(Storage& storage, const IDomain& domain, const BodySettings& body);

    /// Adds a body by explicitly specifying its material.
    /// \copydoc addBody
    BodyView addMonolithicBody(Storage& storage, const IDomain& domain, AutoPtr<IMaterial>&& material);

    BodyView addMonolithicBody(Storage& storage,
        const IDomain& domain,
        AutoPtr<IMaterial>&& material,
        AutoPtr<IDistribution>&& distribution);


    /// \brief Holds data needed to create a single body in \ref addHeterogeneousBody function.
    struct BodySetup {
        AutoPtr<IDomain> domain;
        AutoPtr<IMaterial> material;

        /// Creates a body with undefined domain and material
        BodySetup();

        /// Creates a body by specifying its domain and material
        BodySetup(AutoPtr<IDomain>&& domain, AutoPtr<IMaterial>&& material);

        /// Creates a body by specifying its domain; material is created from parameters in settings
        BodySetup(AutoPtr<IDomain>&& domain, const BodySettings& body);

        /// Move constructor
        BodySetup(BodySetup&& other);

        ~BodySetup();
    };

    /// \brief Creates particles composed of different materials.
    ///
    /// Particles of different materials fill given domains inside the parent (environment) body. Each body
    /// can have different material and have different initial velocity. These bodies don't add more particles
    /// (particle count in settings is irrelevant when creating the bodies), they simply override particles
    /// created by environment body. If multiple bodies overlap, particles are assigned to body listed first
    /// in the array.
    ///
    /// \param storage Particle storage to which the new body is added
    /// \param environment Base body, domain of which defines the body. No particles are generated outside
    ///                    of this domain. By default, all particles have the material given by this body.
    /// \param bodies List of bodies created inside the main environemnt.
    /// \return Array of n+1 BodyViews, where n is the size of \ref bodies parameter. The first one
    ///         corresponds to the environment, the rest are the bodies inside the environment in the
    ///         order they were passed in \ref bodies.
    Array<BodyView> addHeterogeneousBody(Storage& storage,
        BodySetup&& environment,
        ArrayView<BodySetup> bodies);

    /// \brief Creates a rubble-pile body, composing of monolithic spheres.
    ///
    /// The spheres are created randomly, using the RNG from InitialMaterialContext. Each sphere is considered
    /// as a different body, so the interactions between the spheres is the same as the interactions between
    /// an impactor an a target, for example. The spheres can partially exceed the boundary of the domain; the
    /// particles outside of the domain are removed in this case. The body is created 'statically', no
    /// gravitational interaction is considered and the created configuration might not be stable. The body is
    /// created similarly as in \cite Benavidez_2012. For more complex initial conditions of rubble-pile
    /// bodies, see \cite Deller_2017.
    ///
    /// \param storage Particle storage to which the rubble-pile body is added.
    /// \param domain Spatial domain where the particles are placed.
    /// \param sfd Power-law size-frequency distribution of the spheres.
    /// \param bodySettings Material parameters of the spheres.
    ///
    /// \todo potentially move to other object / create an abstract interface for addBody ?
    void addRubblePileBody(Storage& storage,
        const IDomain& domain,
        const PowerLawSfd& sfd,
        const BodySettings& bodySettings);

private:
    void setQuantities(Storage& storage, IMaterial& material, const Float volume);
};

/// \brief Displaces particles so that no two particles overlap.
///
/// In case no particles overlap, function does nothing.
/// \param r Positions of particles
/// \param radius Radius of the particles in units of smoothing length.
void repelParticles(ArrayView<Vector> r, const Float radius);

/// \brief Modifies particle positions so that their center of mass lies at origin.
///
/// Function can be also used for particle velocities, modifying them so that the total momentum is zero.
/// \param m Particle masses; must be positive values
/// \param r Particle positions (or velocities)
void moveToCenterOfMassSystem(ArrayView<const Float> m, ArrayView<Vector> r);

NAMESPACE_SPH_END
