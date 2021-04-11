#include "physics/Functions.h"

NAMESPACE_SPH_BEGIN

Float evalBenzAsphaugScalingLaw(const Float D, const Float rho) {
    const Float D_cgs = 100._f * D;
    const Float rho_cgs = 1.e-3_f * rho;
    //  the scaling law parameters (in CGS units)
    constexpr Float Q_0 = 9.e7_f;
    constexpr Float B = 0.5_f;
    constexpr Float a = -0.36_f;
    constexpr Float b = 1.36_f;

    const Float Q_cgs = Q_0 * pow(D_cgs / 2._f, a) + B * rho_cgs * pow(D_cgs / 2._f, b);
    return 1.e-4_f * Q_cgs;
}

Float getImpactEnergy(const Float R, const Float r, const Float v) {
    return 0.5_f * sphereVolume(r) * sqr(v) / sphereVolume(R);
}

Float getEffectiveImpactArea(const Float R, const Float r, const Float phi) {
    SPH_ASSERT(phi >= 0._f && phi <= PI / 2._f);
    const Float d = (r + R) * sin(phi);
    if (d < R - r) {
        return 1._f;
    } else {
        const Float area = sqr(r) * acos((sqr(d) + sqr(r) - sqr(R)) / (2 * d * r)) +
                           sqr(R) * acos((sqr(d) + sqr(R) - sqr(r)) / (2 * d * R)) -
                           0.5_f * sqrt((R + r - d) * (d + r - R) * (d - r + R) * (d + r + R));
        return area / (PI * sqr(r));
    }
}

Float getImpactorRadius(const Float R_pb, const Float v_imp, const Float QOverQ_D, const Float rho) {
    // for 'regular' impact energy, simply compute the impactor radius by inverting Q=1/2 m_imp v^2/M_pb
    const Float Q_D = evalBenzAsphaugScalingLaw(2._f * R_pb, rho);
    const Float Q = QOverQ_D * Q_D;
    return root<3>(2._f * Q / sqr(v_imp)) * R_pb;
}

Float getImpactorRadius(const Float R_pb,
    const Float v_imp,
    const Float phi,
    const Float Q_effOverQ_D,
    const Float rho) {
    // The effective impact energy depends on impactor radius, which is what we want to compute; needs
    // to be solved by iterative algorithm. First, get an estimate of the radius by using the regular
    // impact energy
    Float r = getImpactorRadius(R_pb, v_imp, Q_effOverQ_D, rho);
    if (almostEqual(getEffectiveImpactArea(R_pb, r, phi), 1._f, 1.e-4_f)) {
        // (almost) whole impactor hits the target, no need to account for effective energy
        return r;
    }
    // Effective energy LOWER than the regular energy, so we only need to increase the impactor, no
    // need to check for smaller values.
    Float lastR = LARGE;
    const Float eps = 1.e-4_f * r;
    while (abs(r - lastR) > eps) {
        lastR = r;
        const Float a = getEffectiveImpactArea(R_pb, r, phi);
        // effective->regular = divide by a
        r = getImpactorRadius(R_pb, v_imp, Q_effOverQ_D / a, rho);
    }
    return r;
}

NAMESPACE_SPH_END
