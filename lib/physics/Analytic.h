#pragma once

/// \file Analytic.h
/// \brief Analytic solutions of some physical quantities
/// \author Pavel Sevecek
/// \date 2016-2017

#include "geometry/Vector.h"
#include "physics/Constants.h"

NAMESPACE_SPH_BEGIN

namespace Analytic {

    /// Properties of a homogeneous sphere in rest (no temporal derivatives)
    class StaticSphere {
    private:
        /// Radius
        Float r0;

        /// Density
        Float rho;

    public:
        StaticSphere(const Float r0, const Float rho)
            : r0(r0)
            , rho(rho) {}

        /// Return the pressure at given radius r of a sphere self-compressed by gravity.
        INLINE Float getPressure(const Float r) const {
            if (r > r0) {
                return 0._f;
            }
            return 2._f / 3._f * PI * Constants::gravity * sqr(rho) * (sqr(r0) - sqr(r));
        }

        /// Returns the gravitational acceleration at given radius r. The acceleration increases linearily up
        /// to r0 and then decreases with r^2.
        INLINE Vector getAcceleration(const Vector& r) const {
            const Float l = getLength(r);
            const Float l0 = min(r0, l);
            return -Constants::gravity * rho * sphereVolume(l0) * r / pow<3>(l);
        }
    };
}

NAMESPACE_SPH_END
