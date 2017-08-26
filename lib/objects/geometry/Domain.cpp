#include "objects/geometry/Domain.h"

NAMESPACE_SPH_BEGIN

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SphericalDomain implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

SphericalDomain::SphericalDomain(const Vector& center, const Float& radius)
    : IDomain(center)
    , radius(radius) {}

Float SphericalDomain::getVolume() const {
    return sphereVolume(radius);
}

Box SphericalDomain::getBoundingBox() const {
    const Vector r(radius);
    return Box(this->center - r, this->center + r);
}

bool SphericalDomain::isInside(const Vector& v) const {
    return isInsideImpl(v);
}

void SphericalDomain::getSubset(ArrayView<const Vector> vs,
    Array<Size>& output,
    const SubsetType type) const {
    switch (type) {
    case SubsetType::OUTSIDE:
        for (Size i = 0; i < vs.size(); ++i) {
            if (!isInsideImpl(vs[i])) {
                output.push(i);
            }
        }
        break;
    case SubsetType::INSIDE:
        for (Size i = 0; i < vs.size(); ++i) {
            if (isInsideImpl(vs[i])) {
                output.push(i);
            }
        }
        break;
    default:
        NOT_IMPLEMENTED;
    }
}

void SphericalDomain::getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const {
    distances.clear();
    for (const Vector& v : vs) {
        const Float dist = radius - getLength(v - this->center);
        distances.push(dist);
    }
}

void SphericalDomain::project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices) const {
    auto impl = [this](Vector& v) {
        if (!isInsideImpl(v)) {
            const Float h = v[H];
            v = getNormalized(v - this->center) * radius + this->center;
            v[H] = h;
        }
    };
    if (indices) {
        for (Size i : indices.value()) {
            impl(vs[i]);
        }
    } else {
        for (Vector& v : vs) {
            impl(v);
        }
    }
}

void SphericalDomain::addGhosts(ArrayView<const Vector> vs,
    Array<Ghost>& ghosts,
    const Float eta,
    const Float eps) const {
    ASSERT(eps < eta);
    ghosts.clear();
    // iterate using indices as the array can reallocate during the loop
    for (Size i = 0; i < vs.size(); ++i) {
        if (!isInsideImpl(vs[i])) {
            continue;
        }
        Float length;
        Vector normalized;
        tieToTuple(normalized, length) = getNormalizedWithLength(vs[i] - this->center);
        const Float h = vs[i][H];
        const Float diff = radius - length;
        if (diff < h * eta) {
            Vector v = vs[i] + max(eps * h, 2._f * diff) * normalized;
            v[H] = h;
            ghosts.push(Ghost{ v, i });
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EllipsoidalDomain implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

EllipsoidalDomain::EllipsoidalDomain(const Vector& center, const Vector& axes)
    : IDomain(center)
    , radii(axes) {
    effectiveRadius = cbrt(radii[X] * radii[Y] * radii[Z]);
    ASSERT(isReal(effectiveRadius));
}

Float EllipsoidalDomain::getVolume() const {
    return sphereVolume(effectiveRadius);
}

Box EllipsoidalDomain::getBoundingBox() const {
    return Box(this->center - radii, this->center + radii);
}

bool EllipsoidalDomain::isInside(const Vector& v) const {
    return isInsideImpl(v);
}

void EllipsoidalDomain::getSubset(ArrayView<const Vector> vs,
    Array<Size>& output,
    const SubsetType type) const {
    switch (type) {
    case SubsetType::OUTSIDE:
        for (Size i = 0; i < vs.size(); ++i) {
            if (!isInsideImpl(vs[i])) {
                output.push(i);
            }
        }
        break;
    case SubsetType::INSIDE:
        for (Size i = 0; i < vs.size(); ++i) {
            if (isInsideImpl(vs[i])) {
                output.push(i);
            }
        }
        break;
    default:
        NOT_IMPLEMENTED;
    }
}

void EllipsoidalDomain::getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const {
    distances.clear();
    for (const Vector& v : vs) {
        /// \todo test
        const Float dist = effectiveRadius * (1.f - getLength((v - this->center) / radii));
        distances.push(dist);
    }
}

void EllipsoidalDomain::project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices) const {
    auto impl = [this](Vector& v) {
        if (!isInsideImpl(v)) {
            const Float h = v[H];
            /// \todo test
            v = getNormalized((v - this->center) / radii) * radii + this->center;
            v[H] = h;
        }
    };
    if (indices) {
        for (Size i : indices.value()) {
            impl(vs[i]);
        }
    } else {
        for (Vector& v : vs) {
            impl(v);
        }
    }
}

void EllipsoidalDomain::addGhosts(ArrayView<const Vector> UNUSED(vs),
    Array<Ghost>& ghosts,
    const Float UNUSED_IN_RELEASE(eta),
    const Float UNUSED_IN_RELEASE(eps)) const {
    ASSERT(eps < eta);
    ghosts.clear();
    NOT_IMPLEMENTED;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// BlockDomain implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////


BlockDomain::BlockDomain(const Vector& center, const Vector& edges)
    : IDomain(center)
    , box(center - 0.5_f * edges, center + 0.5_f * edges) {}

Float BlockDomain::getVolume() const {
    return box.volume();
}

Box BlockDomain::getBoundingBox() const {
    return box;
}

bool BlockDomain::isInside(const Vector& v) const {
    return box.contains(v);
}

void BlockDomain::getSubset(ArrayView<const Vector> vs, Array<Size>& output, const SubsetType type) const {
    switch (type) {
    case SubsetType::OUTSIDE:
        for (Size i = 0; i < vs.size(); ++i) {
            if (!box.contains(vs[i])) {
                output.push(i);
            }
        }
        break;
    case SubsetType::INSIDE:
        for (Size i = 0; i < vs.size(); ++i) {
            if (box.contains(vs[i])) {
                output.push(i);
            }
        }
    }
}

void BlockDomain::getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const {
    distances.clear();
    for (const Vector& v : vs) {
        const Vector d1 = v - box.lower();
        const Vector d2 = box.upper() - v;
        // we cannot just select min element of abs, we need signed distance
        Float minDist = INFTY, minAbsDist = INFTY;
        for (int i = 0; i < 3; ++i) {
            Float dist = abs(d1[i]);
            if (dist < minAbsDist) {
                minAbsDist = dist;
                minDist = d1[i];
            }
            dist = abs(d2[i]);
            if (dist < minAbsDist) {
                minAbsDist = dist;
                minDist = d2[i];
            }
        }
        ASSERT(minDist < INFTY);
        distances.push(minDist);
    }
}

void BlockDomain::project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices) const {
    auto impl = [this](Vector& v) {
        if (!box.contains(v)) {
            const Float h = v[H];
            v = box.clamp(v);
            v[H] = h;
        }
    };
    if (indices) {
        for (Size i : indices.value()) {
            impl(vs[i]);
        }
    } else {
        for (Vector& v : vs) {
            impl(v);
        }
    }
}

void BlockDomain::addGhosts(ArrayView<const Vector> vs,
    Array<Ghost>& ghosts,
    const Float eta,
    const Float eps) const {
    ASSERT(eps < eta);
    ghosts.clear();
    for (Size i = 0; i < vs.size(); ++i) {
        if (!box.contains(vs[i])) {
            continue;
        }
        const Float h = vs[i][H];
        const Vector d1 = max(vs[i] - box.lower(), Vector(eps * h));
        const Vector d2 = max(box.upper() - vs[i], Vector(eps * h));
        // each face for the box can potentially create a ghost
        if (d1[X] < eta * h) {
            Vector v = vs[i] - Vector(2._f * d1[X], 0._f, 0._f);
            v[H] = h;
            ghosts.push(Ghost{ v, i });
        }
        if (d1[Y] < eta * h) {
            Vector v = vs[i] - Vector(0._f, 2._f * d1[Y], 0._f);
            v[H] = h;
            ghosts.push(Ghost{ v, i });
        }
        if (d1[Z] < eta * h) {
            Vector v = vs[i] - Vector(0._f, 0._f, 2._f * d1[Z]);
            v[H] = h;
            ghosts.push(Ghost{ v, i });
        }

        if (d2[X] < eta * h) {
            Vector v = vs[i] + Vector(2._f * d2[X], 0._f, 0._f);
            v[H] = h;
            ghosts.push(Ghost{ v, i });
        }
        if (d2[Y] < eta * h) {
            Vector v = vs[i] + Vector(0._f, 2._f * d2[Y], 0._f);
            v[H] = h;
            ghosts.push(Ghost{ v, i });
        }
        if (d2[Z] < eta * h) {
            Vector v = vs[i] + Vector(0._f, 0._f, 2._f * d2[Z]);
            v[H] = h;
            ghosts.push(Ghost{ v, i });
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CylindricalDomain implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////


CylindricalDomain::CylindricalDomain(const Vector& center,
    const Float radius,
    const Float height,
    const bool includeBases)
    : IDomain(center)
    , radius(radius)
    , height(height)
    , includeBases(includeBases) {}

Float CylindricalDomain::getVolume() const {
    return PI * sqr(radius) * height;
}

Box CylindricalDomain::getBoundingBox() const {
    const Vector sides(radius, radius, 0.5_f * height);
    return Box(this->center - sides, this->center + sides);
}

bool CylindricalDomain::isInside(const Vector& v) const {
    return this->isInsideImpl(v);
}

void CylindricalDomain::getSubset(ArrayView<const Vector> vs,
    Array<Size>& output,
    const SubsetType type) const {
    switch (type) {
    case SubsetType::OUTSIDE:
        for (Size i = 0; i < vs.size(); ++i) {
            if (!isInsideImpl(vs[i])) {
                output.push(i);
            }
        }
        break;
    case SubsetType::INSIDE:
        for (Size i = 0; i < vs.size(); ++i) {
            if (isInsideImpl(vs[i])) {
                output.push(i);
            }
        }
    }
}

void CylindricalDomain::getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const {
    distances.clear();
    for (const Vector& v : vs) {
        const Float dist = radius - getLength(Vector(v[X], v[Y], this->center[Z]) - this->center);
        if (includeBases) {
            distances.push(min(dist, abs(0.5_f * height - abs(v[Z] - this->center[Z]))));
        } else {
            distances.push(dist);
        }
    }
}

void CylindricalDomain::project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices) const {
    auto impl = [this](Vector& v) {
        if (!isInsideImpl(v)) {
            const Float h = v[H];
            v = getNormalized(Vector(v[X], v[Y], this->center[Z]) - this->center) * radius +
                Vector(this->center[X], this->center[Y], v[Z]);
            if (includeBases) {
                v[Z] = clamp(v[Z], this->center[Z] - 0.5_f * height, this->center[Z] + 0.5_f * height);
            }
            v[H] = h;
        }
    };
    if (indices) {
        for (Size i : indices.value()) {
            impl(vs[i]);
        }
    } else {
        for (Vector& v : vs) {
            impl(v);
        }
    }
}

void CylindricalDomain::addGhosts(ArrayView<const Vector> vs,
    Array<Ghost>& ghosts,
    const Float eta,
    const Float eps) const {
    ghosts.clear();
    ASSERT(eps < eta);
    for (Size i = 0; i < vs.size(); ++i) {
        if (!isInsideImpl(vs[i])) {
            continue;
        }
        Float length;
        Vector normalized;
        tieToTuple(normalized, length) =
            getNormalizedWithLength(Vector(vs[i][X], vs[i][Y], this->center[Z]) - this->center);
        const Float h = vs[i][H];
        ASSERT(radius - length >= 0._f);
        Float diff = max(eps * h, radius - length);
        if (diff < h * eta) {
            Vector v = vs[i] + 2._f * diff * normalized;
            v[H] = h;
            ghosts.push(Ghost{ v, i });
        }
        if (includeBases) {
            diff = 0.5_f * height - (vs[i][Z] - this->center[Z]);
            if (diff < h * eta) {
                ghosts.push(Ghost{ vs[i] + Vector(0._f, 0._f, 2._f * diff), i });
            }
            diff = 0.5_f * height - (this->center[Z] - vs[i][Z]);
            if (diff < h * eta) {
                ghosts.push(Ghost{ vs[i] - Vector(0._f, 0._f, 2._f * diff), i });
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// HexagonalDomain implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////


HexagonalDomain::HexagonalDomain(const Vector& center,
    const Float radius,
    const Float height,
    const bool includeBases)
    : IDomain(center)
    , outerRadius(radius)
    , innerRadius(sqrt(0.75_f) * outerRadius)
    , height(height)
    , includeBases(includeBases) {}

Float HexagonalDomain::getVolume() const {
    // 6 equilateral triangles
    return 1.5_f * sqrt(3) * sqr(outerRadius);
}

Box HexagonalDomain::getBoundingBox() const {
    const Vector sides(outerRadius, outerRadius, 0.5_f * height);
    return Box(this->center - sides, this->center + sides);
}

bool HexagonalDomain::isInside(const Vector& v) const {
    return this->isInsideImpl(v);
}

void HexagonalDomain::getSubset(ArrayView<const Vector> vs,
    Array<Size>& output,
    const SubsetType type) const {
    switch (type) {
    case SubsetType::OUTSIDE:
        for (Size i = 0; i < vs.size(); ++i) {
            if (!isInsideImpl(vs[i])) {
                output.push(i);
            }
        }
        break;
    case SubsetType::INSIDE:
        for (Size i = 0; i < vs.size(); ++i) {
            if (isInsideImpl(vs[i])) {
                output.push(i);
            }
        }
    }
}

void HexagonalDomain::getDistanceToBoundary(ArrayView<const Vector>, Array<Float>&) const {
    NOT_IMPLEMENTED;
}

void HexagonalDomain::project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices) const {
    auto impl = [this](Vector& v) {
        if (!isInsideImpl(v)) {
            // find triangle
            const Float phi = atan2(v[Y], v[X]);
            const Float r = outerRadius * hexagon(phi);
            const Vector u = r * getNormalized(Vector(v[X], v[Y], 0._f));
            v = Vector(u[X], u[Y], v[Z], v[H]);
            if (includeBases) {
                v[Z] = clamp(v[Z], this->center[Z] - 0.5_f * height, this->center[Z] + 0.5_f * height);
            }
        }
    };
    if (indices) {
        for (Size i : indices.value()) {
            impl(vs[i]);
        }
    } else {
        for (Vector& v : vs) {
            impl(v);
        }
    }
}

void HexagonalDomain::addGhosts(ArrayView<const Vector> vs,
    Array<Ghost>& ghosts,
    const Float eta,
    const Float eps) const {
    ghosts.clear();
    ASSERT(eps < eta);
    /// \todo almost identical to cylinder domain, remove duplication
    for (Size i = 0; i < vs.size(); ++i) {
        if (!isInsideImpl(vs[i])) {
            continue;
        }
        Float length;
        Vector normalized;
        tieToTuple(normalized, length) =
            getNormalizedWithLength(Vector(vs[i][X], vs[i][Y], this->center[Z]) - this->center);
        const Float h = vs[i][H];
        ASSERT(outerRadius - length >= 0._f);
        const Float phi = atan2(vs[i][Y], vs[i][X]);
        const Float r = outerRadius * hexagon(phi);
        Float diff = max(eps * h, r - length);
        if (diff < h * eta) {
            Vector v = vs[i] + 2._f * diff * normalized;
            v[H] = h;
            ghosts.push(Ghost{ v, i });
        }
        if (includeBases) {
            diff = 0.5_f * height - (vs[i][Z] - this->center[Z]);
            if (diff < h * eta) {
                ghosts.push(Ghost{ vs[i] + Vector(0._f, 0._f, 2._f * diff), i });
            }
            diff = 0.5_f * height - (this->center[Z] - vs[i][Z]);
            if (diff < h * eta) {
                ghosts.push(Ghost{ vs[i] - Vector(0._f, 0._f, 2._f * diff), i });
            }
        }
    }
}


NAMESPACE_SPH_END
