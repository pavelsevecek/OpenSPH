#pragma once

/// \file Analysis.h
/// \brief Various function for interpretation of the results of a simulation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/finders/INeighbourFinder.h"
#include "objects/wrappers/Expected.h"
#include "objects/wrappers/Function.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class Storage;
class Path;
class ILogger;

namespace Post {

    enum class ComponentConnectivity {
        /// Overlapping particles belong into the same component
        OVERLAP,

        /// Particles belong to the same component if they overlap and the have the same flag.
        SEPARATE_BY_FLAG,

        /// Particles overlap or their relative velocity is less than the escape velocity
        ESCAPE_VELOCITY,
    };

    /// \brief Finds and marks connected components (a.k.a. separated bodies) in the array of vertices.
    ///
    /// \param storage Storage containing the particles. Must also contain QuantityId::FLAG if
    ///                SEPARATE_BY_FLAG option is used.
    /// \param particleRadius Size of particles in smoothing lengths.
    /// \param connectivity Defines additional conditions of particle separation (besides their distance).
    /// \param indices[out] Array of indices from 0 to n-1, where n is the number of components. In the array,
    ///                     i-th index corresponds to component to which i-th particle belongs.
    /// \return Number of components
    Size findComponents(const Storage& storage,
        const Float particleRadius,
        const ComponentConnectivity connectivity,
        Array<Size>& indices);

    /// \todo docs
    ///
    /// \todo escape velocity
    Storage findFutureBodies(const Storage& storage, const Float particleRadius, ILogger& logger);

    /// \todo TEMPORARY FUNCTION, REMOVE
    Storage findFutureBodies2(const Storage& storage, ILogger& logger);

    /// \brief Computes the total inertia tensor of particles with respect to given center
    SymmetricTensor getInertiaTensor(ArrayView<const Float> m, ArrayView<const Vector> r, const Vector& r0);

    /// \brief Computes the total inertia tensor of particle with respect to their center of mass.
    SymmetricTensor getInertiaTensor(ArrayView<const Float> m, ArrayView<const Vector> r);

    /// Potential relationship of the body with a respect to the largest remnant (fragment).
    enum class MoonEnum {
        LARGEST_FRAGMENT, ///< This is the largest fragment (or remnant, depending on definition)
        RUNAWAY,      ///< Body is on hyperbolic trajectory, ejected away from the largest remnant (fragment)
        MOON,         ///< Body is on elliptical trajectory, it is a potential sattelite
        IMPACTOR,     ///< Body is on collisional course with the largest remnant (fragment)
        UNOBSERVABLE, ///< Body is smaller than the user-defined observational limit
    };

    /// \brief Find a potential satellites of the largest body.
    ///
    /// The sattelites are mainly determined using the total energy of the bodies; if the energy is negative,
    /// the body is likely to be a sattelite. Note that the N-body interactions can eject bodies with negative
    /// energy away from the largest remnant (fragment), and also make a body with positive energy a future
    /// satellite (by decreasing the energy during close encounter with 3rd body). If the energy of a body is
    /// negative, a Keplerian ellipse (assuming 2-body problem) is computed and based on this ellipse, the
    /// body is either marked as moon or as impactor.
    ///
    /// The function is mainly intended for post-reaccumulation analysis of the created asteroidal family.
    /// Satellites determined during reaccumulation can be very uncertain, as explained above. The storage
    /// must contains positions (with velocities) and masses of the bodies. The bodies do not have to be
    /// sorted, the largest remnant (or fragment) is defined as the most massive body.
    ///
    /// \param storage Storage containing bodies of the asteroidal family
    /// \param radius Relative radius of bodies, multiplying the radius stored in h-component. Used only
    ///               in collision detection; value 0 turns it off completely.
    /// \param limit Relative observational limit, with a respect to the largest remnant (fragment). Bodies
    ///              with radius smaller than \f${\rm limit} \cdot R_{\rm lr}\f$ are marked as UNOBSERVABLE,
    ///              regardless their bound status. Must be in interval [0, 1>.
    /// \return Array of the same size of storage, marking each body in the storage; see MoonEnum.
    Array<MoonEnum> findMoons(const Storage& storage, const Float radius = 1._f, const Float limit = 0._f);


    /// \brief Object holding Keplerian orbital elements of a body
    ///
    /// Contains necessary information to determine the orbit of a body. We use the angular momentum and
    /// Laplace vector to avoid problems with singular cases (e=0, I=0); the argument of periapsis and the
    /// longitude of the ascending node can be computed by calling member functions.
    struct KeplerianElements {
        Float a; ///< Semi-major axis
        Float e; ///< Excentricity
        Float i; ///< Inclination with respect to the z=0 plane

        Vector L; ///< Angular momentum, perpendicular to the orbital plane
        Vector K; ///< Laplace vector, integral of motion with direction towards pericenter


        /// Computes the argument of periapsis of the orbit. In the singular case e=0, returns 0.
        Float periapsisArgument() const;

        /// Computes the longitude of the ascending node. In the singular case i=0, reutrns 0.
        Float ascendingNode() const;

        /// Computes the distance of the pericenter
        Float pericenterDist() const;

        /// Computes the semi-minor axis
        Float semiminorAxis() const;
    };

    /// \brief Computes the orbital elements, given position and velocity of a body.
    ///
    /// If the body trajectory is not closed (hyperbolic motion), returns NOTHING.
    /// \param M Mass characterizing the gravitational field, or sum of body masses for two-body problem
    /// \param mu Mass of the orbiting body, or reduced mass for two-body problem
    /// \param r Position of the orbiting body
    /// \param v Velocity of the orbiting body
    Optional<KeplerianElements> findKeplerEllipse(const Float M,
        const Float mu,
        const Vector& r,
        const Vector& v);

    /// \brief Quantity from which the histogram is constructed
    ///
    /// Beside the listed options, any QuantityId can be used, by casting to HistogramId enum. All values of
    /// HistogramId has to be negative to avoid clashes.
    enum class HistogramId {
        /// Particle radii or equivalent radii of compoennts
        RADII = -1,

        /// Particle velocities
        VELOCITIES = -2,

        /// Angular velocities of particles
        ANGULAR_VELOCITIES = -3,
    };

    /// Parameters of the histogram
    struct HistogramParams {

        /// \brief Source data used to construct the histogram.
        enum class Source {
            /// Equivalent radii of connected chunks of particles (SPH framework)
            COMPONENTS,

            /// Radii of individual particles, considering particles as spheres (N-body framework)
            PARTICLES,
        };

        Source source = Source::PARTICLES;

        HistogramId id = HistogramId::RADII;

        /// Range of values from which the histogram is constructed. Unbounded range means the range is
        /// selected based on the source data.
        Interval range;

        /// Number of histogram bins. 0 means the number is selected based on the source data. Used only by
        /// differential SFD.
        Size binCnt = 0;

        /// Parameters used by histogram of components
        struct ComponentParams {
            Float radius = 2._f;
        } components;

        /// Validator used to determine particles/component included in the histogram
        ///
        /// By default, all particles/components are included
        Function<bool(const Float value)> validator = [](const Float UNUSED(value)) { return true; };
    };

    /// Point in SFD
    struct SfdPoint {
        /// Radius or value of measured quantity (x coordinate in the plot)
        Float value;

        /// Number of particles/components
        Size count;
    };

    /// \brief Computes differential size-frequency distribution of particle radii.
    Array<SfdPoint> getDifferentialSfd(const Storage& storage, const HistogramParams& params);

    /// \brief Computes cummulative size-frequency distribution of body sizes (equivalent diameters).
    ///
    /// The storage must contain at least particle positions, masses and densities.
    /// \param params Parameters of the histogram.
    Array<SfdPoint> getCummulativeSfd(const Storage& storage, const HistogramParams& params);


    /// \brief Parses the pkdgrav output file and creates a storage with quantities stored in the file.
    ///
    /// Pkdgrav output contains particle positions, their velocities, angular velocities and their masses.
    /// Parsed storage is constructed with these quantities, the radii of the spheres are saved as
    /// H-component (however the radius is NOT equal to the smoothing length). The particles are sorted in the
    /// storage according to their masses, in descending order. The largest remnant (if exists) or largest
    /// fragment is therefore particle with index 0.
    /// \param path Path to the file. The extension of the file shall be ".bt".
    /// \return Storage with created quantities or error message.
    Expected<Storage> parsePkdgravOutput(const Path& path);
} // namespace Post

NAMESPACE_SPH_END
