#pragma once

/// \file Function.h
/// \brief Helper functions used by the code
/// \author Pavel Sevecek
/// \date 2016-2017

#include "math/rng/Rng.h"
#include "objects/geometry/Vector.h"
#include "physics/Constants.h"

NAMESPACE_SPH_BEGIN

/// Contains analytic solutions of equations
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
} // namespace Analytic


/// \todo docs
/// \todo replace D with units, do not enforce SI
INLINE Float evalBenzAsphaugScalingLaw(const Float D, const Float rho) {
    const Float D_cgs = 100._f * D;
    const Float rho_cgs = 1.e-3_f * rho;
    //  the scaling law parameters (in CGS units)
    constexpr Float Q_0 = 9.e7_f;
    constexpr Float B = 0.5_f;
    constexpr Float a = -0.36_f;
    constexpr Float b = 1.36_f;

    return Q_0 * pow(D_cgs / 2._f, a) + B * rho_cgs * pow(D_cgs / 2._f, b);
}

class CollisionMC {
private:
    UniformRng rng;
    Float M_tot;

public:
    Float M_LR(const Float QoverQ_D) {
        if (QoverQ_D < 1._f) {
            return (-0.5_f * (QoverQ_D - 1._f) + 0.5_f) * M_tot;
        } else {
            return (-0.35_f * (QoverQ_D - 1._f) + 0.5_f) * M_tot;
        }
    }

    Float M_LF(const Float QoverQ_D) {
        return 8.e-3_f * (QoverQ_D * exp(-sqr(0.25_f * QoverQ_D))) * M_tot;
    }

    Float q(const Float QoverQ_D) {
        return -10._f + 7._f * pow(QoverQ_D, 0.4_f) * exp(-QoverQ_D / 7._f);
    }
};

NAMESPACE_SPH_END
