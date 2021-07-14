#pragma once

#include "objects/geometry/Vector.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

namespace Kepler {

/// \brief Object holding Keplerian orbital elements of a body
///
/// Contains necessary information to determine the orbit of a body. We use the angular momentum and
/// Laplace vector to avoid problems with singular cases (e=0, I=0); the argument of periapsis and the
/// longitude of the ascending node can be computed by calling member functions.
struct Elements {
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
Optional<Elements> computeOrbitalElements(const Float M, const Float mu, const Vector& r, const Vector& v);

/// \brief Computes the eccentric anomaly by solving the Kepler's equation.
///
/// \param M Mean anomaly
/// \param e Eccentricity
Float solveKeplersEquation(const Float M, const Float e, const Size iterCnt = 10);

/// \brief Computes the true anomaly from the eccentric anomaly and the eccentricity.
Float eccentricAnomalyToTrueAnomaly(const Float u, const Float e);

/// \brief Computes the eccentric anomaly from the true anomaly and the eccentricity.
Float trueAnomalyToEccentricAnomaly(const Float v, const Float e);

/// \brief Computes the position on the elliptic trajectory.
///
/// It assumes a planar motion in z=0 plane.
/// \param a Semi-major axis
/// \param e Eccentricity
/// \param u Eccentric anomaly
Vector position(const Float a, const Float e, const Float u);

/// \brief Computes the velocity vector on the elliptic trajectory.
///
/// It assumes a planar motion in z=0 plane.
/// \param a Semi-major axis
/// \param e Eccentricity
/// \param u Eccentric anomaly
/// \param n Mean motion
Vector velocity(const Float a, const Float e, const Float u, const Float n);

/// \brief Computes the mean motion from the Kepler's 3rd law.
Float meanMotion(const Float a, const Float m_total);

} // namespace Kepler

NAMESPACE_SPH_END
