#pragma once

#include "math/MathUtils.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

namespace Relativity {

struct Vector4 {
    Float t, r, theta, phi;
};

struct Geodesic {
    Array<Vector4> points;
};

Geodesic solve(const Vector4& p0, const Vector4& dp0, const Float k, const Float dlambda) {
    // https://ned.ipac.caltech.edu/level5/March01/Carroll3/Carroll7.html

    Float t = p0.t;
    Float r = p0.r;
    Float theta = p0.theta;
    Float phi = p0.phi;

    Float dt = dp0.t;
    Float dr = dp0.r;
    Float dtheta = dp0.theta;
    Float dphi = dp0.phi;

    while (true) {
        const Float d2t = -2 * k / (r * (r - 2 * k)) * dr * dt;
        const Float d2r = -k / pow<3>(r) * (r - 2 * k) * sqr(dt) + k / (r * (r - 2 * k)) * sqr(r) +
                          (r - 2 * k) * (sqr(dtheta) + sqr(sin(theta)) * sqr(dphi));
        const Float d2theta = -2 / r * dtheta * dr + sin(theta) * cos(theta) * sqr(dphi);
        const Float d2phi = -2 / r * dphi * dr - 2 * cos(theta) / sin(theta) * dtheta * dphi;

        dt += d2t * dlambda;
        dr += d2r * dlambda;
        dtheta += d2theta * dlambda;
        dphi += d2phi * dlambda;

        t += dt * dlambda;
        r += dr * dlambda;
        theta += dtheta * dlambda;
        phi += dphi * dlambda;
    }
}

} // namespace Relativity

NAMESPACE_SPH_END
