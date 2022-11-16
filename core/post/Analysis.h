#pragma once

/// \file Analysis.h
/// \brief Various function for interpretation of the results of a simulation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/finders/NeighborFinder.h"
#include "objects/wrappers/Expected.h"
#include "objects/wrappers/ExtendedEnum.h"
#include "objects/wrappers/Function.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

class Storage;
class Path;
class ILogger;
struct PlotPoint;
enum class QuantityId;
namespace Post {
enum class HistogramId;
}

SPH_EXTEND_ENUM(QuantityId, Post::HistogramId);

namespace Post {

/// \brief Finds the number of neighbors of each particle.
///
/// Note that each particle searches neighbors up to the distance given by their smoothing length, so the
/// relation "A is a neighbor of B" might not be symmetrical.
/// \param storage Storage containing the particles.
/// \param particleRadius Size of particles in smoothing lengths.
/// \return Number of neighbors of each particle.
Array<Size> findNeighborCounts(const Storage& storage, const Float particleRadius);


enum class ComponentFlag {
    /// Specifies that overlapping particles belong into the same component
    OVERLAP = 0,

    /// Specifies that particles with different flag belong to different component, even if they overlap.
    SEPARATE_BY_FLAG = 1 << 0,

    /// Specifies that the gravitationally bound particles belong to the same component. If used, the
    /// components are constructed in two steps; first, the initial components are created from overlapping
    /// particles. Then each pair of components is merged if their relative velocity is less then the escape
    /// velocity.
    ESCAPE_VELOCITY = 1 << 1,

    /// If used, components are sorted by the total mass of the particles, i.e. component with index 0 will
    /// correspond to the largest remnant, index 1 will be the second largest body, etc.
    SORT_BY_MASS = 1 << 2,
};

/// \brief Finds and marks connected components (a.k.a. separated bodies) in the array of vertices.
///
/// It is a useful function for finding bodies in simulations where we do not merge particles.
/// \param storage Storage containing the particles. Must also contain QuantityId::FLAG if
///                SEPARATE_BY_FLAG option is used.
/// \param particleRadius Size of particles in smoothing lengths.
/// \param flags Flags specifying connectivity of components, etc; see emum \ref ComponentFlag.
/// \param indices[out] Array of indices from 0 to n-1, where n is the number of components. In the array,
///                     i-th index corresponds to component to which i-th particle belongs.
/// \return Number of components
Size findComponents(const Storage& storage,
    const Float particleRadius,
    const Flags<ComponentFlag> flags,
    Array<Size>& indices);

/// \brief Checks if two particles belong to the same component
struct IComponentChecker : public Polymorphic {
    virtual bool belong(const Size i, const Size j) const = 0;
};

/// \brief Finds and marks connected components (a.k.a. separated bodies) in the array of vertices.
///
/// Overload with a generic checker that returns whether two particles belong to the same component.
Size findComponents(const Storage& storage,
    const Float particleRadius,
    const IComponentChecker& checker,
    Array<Size>& indices);

/// \brief Returns the indices of particles belonging to the largest remnant.
///
/// The returned indices are sorted.
Array<Size> findLargestComponent(const Storage& storage,
    const Float particleRadius,
    const Flags<ComponentFlag> flags);


struct Tumbler {
    /// Index of particle (body)
    Size index;

    /// Angle between the current angular velocity and the angular momentum
    Float beta;
};

/// \brief Find all tumbling asteroids.
///
/// Limit specifies the required misalignment angle (in radians) for asteroid to be considered a tumbler.
/// Note that tumbling begins to be detectable for the misalignment angle larger than 15 degrees with high
/// accuracy data (Henych, 2013).
Array<Tumbler> findTumblers(const Storage& storage, const Float limit);

/// \brief Potential relationship of the body with a respect to the largest remnant (fragment).
enum class MoonEnum {
    LARGEST_FRAGMENT, ///< This is the largest fragment (or remnant, depending on definition)
    RUNAWAY,          ///< Body is on hyperbolic trajectory, ejected away from the largest remnant (fragment)
    MOON,             ///< Body is on elliptical trajectory, it is a potential sattelite
    IMPACTOR,         ///< Body is on collisional course with the largest remnant (fragment)
    UNOBSERVABLE,     ///< Body is smaller than the user-defined observational limit
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

/// \brief Find the number of moons of given body.
///
/// \param m Masses of bodies, sorted in descending order.
/// \param r Positions and radii of bodies, sorted by mass (in descending order)
/// \param v Velocities of bodies, sorted by mass (in descending order)
/// \param i Index of the queried body
/// \param radius Radius multiplier, may be used to exclude moons with pericenter very close to the body.
/// \param limit Limiting mass radio, moons with masses lower than limit*m[i] are excluded.
Size findMoonCount(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    ArrayView<const Vector> v,
    const Size i,
    const Float radius = 1._f,
    const Float limit = 0._f);

/// \brief Computes the center of mass.
Vector getCenterOfMass(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    ArrayView<const Size> idxs = nullptr);

/// \brief Computes the total inertia tensor of particles with respect to given center.
SymmetricTensor getInertiaTensor(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    const Vector& r0,
    ArrayView<const Size> idxs = nullptr);

/// \brief Computes the total inertia tensor of particle with respect to their center of mass.
SymmetricTensor getInertiaTensor(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    ArrayView<const Size> idxs = nullptr);

/// \brief Computes the immediate vector of angular frequency of a rigid body.
Vector getAngularFrequency(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    ArrayView<const Vector> v,
    const Vector& r0,
    const Vector& v0,
    ArrayView<const Size> idxs = nullptr);

Vector getAngularFrequency(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    ArrayView<const Vector> v,
    ArrayView<const Size> idxs = nullptr);

/// \brief Computes the sphericity coefficient of a body.
///
/// Sphericity is defined as the ratio of the surface area of a sphere (with the same volume as the given
/// body) to the surface area of the body (\cite Wadell_1935). This value is equal to 1 for sphere (and only
/// for sphere); the lower the value, the more the body is "aspheric". The sphericity is useful for
/// quantification of "roundness"; the ratio of semi-axes, for example, can be easily 1 even for cube,
/// definitely not a round body.
///
/// Note that the sphericity depends on resolution, small-scale topograpy will always cause the sphericity to
/// decrease! When comparing sphericities, it is necessary to use the same resolution for the compared bodies.
/// \param scheduler Scheduler potentially used for parallelization.
/// \param storage Storage containing particle positions, optionally also masses and densities.
/// \param resolution Relative resolution used to compute the sphericity.
/// \return Wadell's sphericity value for the body, or NOTHING if it cannot be calculated.
Optional<Float> getSphericity(IScheduler& scheduler, const Storage& strorage, const Float resolution = 0.05_f);

/// \brief Quantity from which the histogram is constructed
///
/// Beside the listed options, any QuantityId can be used, by casting to HistogramId enum. All values of
/// HistogramId has to be negative to avoid clashes.
enum class HistogramId {
    /// Particle radii or equivalent radii of components
    RADII = -1,

    /// Radii determined from particle masses and given reference density. This generally given more
    /// 'discretized' SFD, as masses of SPH particles are constant during the run, whereas radii
    /// (smoothing lenghts) or densities may change.
    EQUIVALENT_MASS_RADII = -2,

    /// Particle velocities
    VELOCITIES = -3,

    /// Rotational frequency in revs/day
    ROTATIONAL_FREQUENCY = -4,

    /// Rotational periods of particles (in hours)
    /// \todo it should be in code units and then converted to hours on output!
    ROTATIONAL_PERIOD = -5,

    /// Distribution of axis directions, from -pi to pi
    ROTATIONAL_AXIS = -6,
};

using ExtHistogramId = ExtendedEnum<HistogramId>;

/// \brief Source data used to construct the histogram.
enum class HistogramSource {
    /// Equivalent radii of connected chunks of particles (SPH framework)
    COMPONENTS,

    /// Radii of individual particles, considering particles as spheres (N-body framework)
    PARTICLES,
};


/// \brief Parameters of the histogram
struct HistogramParams {

    /// \brief Range of values from which the histogram is constructed.
    ///
    /// Unbounded range means the range is selected based on the source data.
    Interval range;

    /// \brief Number of histogram bins.
    ///
    /// 0 means the number is selected based on the source data. Used only by differential SFD.
    Size binCnt = 0;

    /// \brief Reference density, used when computing particle radii from their masses.
    Float referenceDensity = 2700._f;

    /// \brief Cutoff value (lower bound) of particle mass for inclusion in the histogram.
    ///
    /// Particles with masses below this value are considered "below observational limit".
    /// Applicable for both component and particle histogram.
    Float massCutoff = 0._f;

    /// \brief Cutoff value (upper bound) of particle velocity for inclusion in the histogram
    ///
    /// Particles moving faster than the cutoff are considered as fragments of projectile and excluded from
    /// histogram, as they are (most probably) not part of any observed family. Applicable for both component
    /// and particle histogram.
    Float velocityCutoff = INFTY;

    /// If true, the bin values of the differential histogram are in the centers of the corresponding
    /// intervals, otherwise they correspond to the lower bound of the interval.
    bool centerBins = true;

    /// \brief Parameters used by histogram of components
    struct ComponentParams {

        /// Radius of particles in units of their smoothing lengths.
        Float radius = 2._f;

        /// Determines how the particles are clustered into the components.
        Flags<Post::ComponentFlag> flags = Post::ComponentFlag::OVERLAP;

    } components;

    /// \brief Function used for inclusiong/exclusion of values in the histogram.
    ///
    /// Works only for particle histograms.
    Function<bool(Size index)> validator = [](Size UNUSED(index)) { return true; };
};

/// \brief Point in the histogram
struct HistPoint {
    /// Value of the quantity
    Float value;

    /// Number of particles/components
    Size count;

    bool operator==(const HistPoint& other) const;
};

/// \brief Computes the differential histogram from given values.
///
/// Note that only bin count and input range are used from the histogram parameters. No cut-offs are applied.
Array<HistPoint> getDifferentialHistogram(ArrayView<const Float> values, const HistogramParams& params);

/// \brief Computes the differential histogram of particles in the storage.
Array<HistPoint> getDifferentialHistogram(const Storage& storage,
    const ExtHistogramId id,
    const HistogramSource source,
    const HistogramParams& params);

/// \brief Computes cumulative (integral) histogram of particles in the storage.
///
/// \param storage Storage containing particle data.
/// \param id Specifies the quantity for which the histogram is constructed.
/// \param source Specifies the input bodies, see \ref HistogramSource.
/// \param params Parameters of the histogram.
Array<HistPoint> getCumulativeHistogram(const Storage& storage,
    const ExtHistogramId id,
    const HistogramSource source,
    const HistogramParams& params);


/// \brief Class representing an ordinary 1D linear function
class LinearFunction {
private:
    Float a, b;

public:
    /// \brief Creates a new linear function.
    ///
    /// \param slope Slope of the function (atan of the angle between the line and the x-axis).
    /// \param offset Offset in y-direction (value of the function for x=0).
    LinearFunction(const Float slope, const Float offset)
        : a(slope)
        , b(offset) {}

    /// \brief Evaluates the linear function for given value.
    INLINE Float operator()(const Float x) const {
        return a * x + b;
    }

    /// \brief Returns the slope of the function.
    Float slope() const {
        return a;
    }

    /// \brief Returns the offset in y-direction.
    Float offset() const {
        return b;
    }

    /// \brief Finds a value of x such that f(x) = y for given y.
    ///
    /// Slope of the function must not be zero.
    Float solve(const Float y) const {
        SPH_ASSERT(a != 0._f);
        return (y - b) / a;
    }
};

/// \brief Finds a linear fit to a set of points.
///
/// The set of points must have at least two elements and they must not coincide.
/// \return Function representing the linear fit.
LinearFunction getLinearFit(ArrayView<const PlotPoint> points);


class QuadraticFunction {
private:
    Float a, b, c;

public:
    /// y = a*x^2 + b*x + c
    QuadraticFunction(const Float a, const Float b, const Float c)
        : a(a)
        , b(b)
        , c(c) {}

    INLINE Float operator()(const Float x) const {
        return (a * x + b) * x + c;
    }

    Float quadratic() const {
        return a;
    }
    Float linear() const {
        return b;
    }
    Float constant() const {
        return c;
    }

    /// \brief Returns solutions of a quadratic equation y = a*x^2 + b*x + c
    ///
    /// Returned array contains 0, 1 or 2 values, depending on the number of real solutions of the equation.
    /// If two solutions exist, the first element of the array is always the smaller solution.
    StaticArray<Float, 2> solve(const Float y) const {
        SPH_ASSERT(a != 0);
        const Float disc = sqr(b) - 4._f * a * (c - y);
        if (disc < 0._f) {
            return EMPTY_ARRAY;
        } else if (disc == 0._f) {
            return { -b / (2._f * a) };
        } else {
            const Float sqrtDisc = sqrt(disc);
            Float x1 = (-b - sqrtDisc) / (2._f * a);
            Float x2 = (-b + sqrtDisc) / (2._f * a);
            if (x1 > x2) {
                std::swap(x1, x2);
            }
            return { x1, x2 };
        }
    }
};

QuadraticFunction getQuadraticFit(ArrayView<const PlotPoint> points);

} // namespace Post

NAMESPACE_SPH_END
