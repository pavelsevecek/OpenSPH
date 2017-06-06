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
        INLINE Float getPressure(const Float r) {
            ASSERT(r <= 1.01_f * r0); // allow a little bit more, due to particle ordering
            if (r > r0) {
                return 0._f;
            }
            return 2._f / 3._f * PI * Constants::gravity * sqr(rho) * (sqr(r0) - sqr(r));
        }

        /// Returns the gravitational acceleration at given radius r.
        INLINE Vector getAcceleration(const Vector& r) {
            return -Constants::gravity * rho * sphereVolume(getLength(r)) * r / pow<3>(getLength(r));
        }
    };
}

NAMESPACE_SPH_END
