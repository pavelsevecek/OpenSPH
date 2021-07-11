#include "post/TwoBody.h"
#include "physics/Constants.h"

NAMESPACE_SPH_BEGIN

Float Kepler::Elements::ascendingNode() const {
    if (sqr(L[Z]) > (1._f - EPS) * getSqrLength(L)) {
        // Longitude of the ascending node undefined, return 0 (this is a valid case, not an error the data)
        return 0._f;
    } else {
        return -atan2(L[X], L[Y]);
    }
}

Float Kepler::Elements::periapsisArgument() const {
    if (e < EPS) {
        return 0._f;
    }
    const Float Omega = this->ascendingNode();
    const Vector OmegaDir(cos(Omega), sin(Omega), 0._f); // direction of the ascending node
    const Float omega = acos(dot(OmegaDir, getNormalized(K)));
    if (K[Z] < 0._f) {
        return 2._f * PI - omega;
    } else {
        return omega;
    }
}

Float Kepler::Elements::pericenterDist() const {
    return a * (1._f - e);
}

Float Kepler::Elements::semiminorAxis() const {
    return a * sqrt(1._f - e * e);
}

Optional<Kepler::Elements> Kepler::computeOrbitalElements(const Float M,
    const Float mu,
    const Vector& r,
    const Vector& v) {
    const Float E = 0.5_f * mu * getSqrLength(v) - Constants::gravity * M * mu / getLength(r);
    if (E >= 0._f) {
        // parabolic or hyperbolic trajectory
        return NOTHING;
    }

    // http://sirrah.troja.mff.cuni.cz/~davok/scripta-NB1.pdf
    Elements elements;
    elements.a = -Constants::gravity * mu * M / (2._f * E);

    const Vector L = mu * cross(r, v); // angular momentum
    SPH_ASSERT(L != Vector(0._f));
    elements.i = acos(L[Z] / getLength(L));
    elements.e = sqrt(1._f + 2._f * E * getSqrLength(L) / (sqr(Constants::gravity) * pow<3>(mu) * sqr(M)));

    elements.K = cross(v, L) - Constants::gravity * mu * M * getNormalized(r);
    elements.L = L;

    return elements;
}

Float Kepler::solveKeplersEquation(const Float M, const Float e, const Size iterCnt) {
    Float E = M;
    for (Size iter = 0; iter < iterCnt; ++iter) {
        E = E - (E - e * sin(E) - M) / (1._f - e * cos(E));
    }
    return E;
}

Float Kepler::eccentricAnomalyToTrueAnomaly(const Float u, const Float e) {
    const Float cosv = (cos(u) - e) / (1 - e * cos(u));
    const Float sinv = sqrt(1 - sqr(e)) * sin(u) / (1._f - e * cos(u));
    return atan2(sinv, cosv);
}

Float Kepler::trueAnomalyToEccentricAnomaly(const Float v, const Float e) {
    const Float cosu = (e + cos(v)) / (1 + e * cos(v));
    const Float sinu = sqrt(1 - sqr(e)) * sin(v) / (1 + e * cos(v));
    return atan2(sinu, cosu);
}

Vector Kepler::position(const Float a, const Float e, const Float u) {
    return a * Vector(cos(u) - e, sqrt(1 - sqr(e)) * sin(u), 0);
}

Vector Kepler::velocity(const Float a, const Float e, const Float u, const Float n) {
    return n * a / (1 - e * cos(u)) * Vector(-sin(u), sqrt(1 - sqr(e)) * cos(u), 0);
}

Float Kepler::meanMotion(const Float a, const Float m_total) {
    return sqrt(Constants::gravity * m_total / pow<3>(a));
}

NAMESPACE_SPH_END
