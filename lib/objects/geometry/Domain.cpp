#include "objects/geometry/Domain.h"

NAMESPACE_SPH_BEGIN

//-----------------------------------------------------------------------------------------------------------
// SphericalDomain implementation
//-----------------------------------------------------------------------------------------------------------

SphericalDomain::SphericalDomain(const Vector& center, const Float& radius)
    : center(center)
    , radius(radius) {}

Vector SphericalDomain::getCenter() const {
    return center;
}

Float SphericalDomain::getVolume() const {
    return sphereVolume(radius);
}

Box SphericalDomain::getBoundingBox() const {
    const Vector r(radius);
    return Box(this->center - r, this->center + r);
}

bool SphericalDomain::contains(const Vector& v) const {
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
            v = getNormalized(v - this->center) * (1._f - EPS) * radius + this->center;
            v[H] = h;
            ASSERT(isInsideImpl(v)); // these asserts are quite reduntant since we have unit tests for that
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

//-----------------------------------------------------------------------------------------------------------
// EllipsoidalDomain implementation
//-----------------------------------------------------------------------------------------------------------

EllipsoidalDomain::EllipsoidalDomain(const Vector& center, const Vector& axes)
    : center(center)
    , radii(axes) {
    effectiveRadius = cbrt(radii[X] * radii[Y] * radii[Z]);
    ASSERT(isReal(effectiveRadius));
}

Vector EllipsoidalDomain::getCenter() const {
    return center;
}

Float EllipsoidalDomain::getVolume() const {
    return sphereVolume(effectiveRadius);
}

Box EllipsoidalDomain::getBoundingBox() const {
    return Box(center - radii, center + radii);
}

bool EllipsoidalDomain::contains(const Vector& v) const {
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
        const Float dist = effectiveRadius * (1._f - getLength((v - this->center) / radii));
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
            ASSERT(isInsideImpl(v));
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
    const Float eta,
    const Float eps) const {
    ASSERT(eps < eta);
    ghosts.clear();
    NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------------------------------------
// BlockDomain implementation
//-----------------------------------------------------------------------------------------------------------

BlockDomain::BlockDomain(const Vector& center, const Vector& edges)
    : box(center - 0.5_f * edges, center + 0.5_f * edges) {}

Vector BlockDomain::getCenter() const {
    return box.center();
}

Float BlockDomain::getVolume() const {
    return box.volume();
}

Box BlockDomain::getBoundingBox() const {
    return box;
}

bool BlockDomain::contains(const Vector& v) const {
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
            ASSERT(box.contains(v));
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
        const Float limitSqr = sqr(eta * h);

        const Vector d1 = max(vs[i] - box.lower(), Vector(eps * h));
        const Vector d2 = max(box.upper() - vs[i], Vector(eps * h));


        // each face for the box can potentially create a ghost
        for (Vector x : { Vector(-d1[X], 0._f, 0._f), Vector(0._f), Vector(d2[X], 0._f, 0._f) }) {
            for (Vector y : { Vector(0._f, -d1[Y], 0._f), Vector(0._f), Vector(0._f, d2[Y], 0._f) }) {
                for (Vector z : { Vector(0._f, 0._f, -d1[Z]), Vector(0._f), Vector(0._f, 0._f, d2[Z]) }) {
                    if (getSqrLength(x) < limitSqr && getSqrLength(y) < limitSqr &&
                        getSqrLength(z) < limitSqr) {
                        const Vector offset = x + y + z;
                        if (offset == Vector(0._f)) {
                            continue;
                        }
                        Vector v = vs[i] + 2._f * offset;
                        v[H] = h;
                        ghosts.push(Ghost{ v, i });
                    }
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------------------------------------
// CylindricalDomain implementation
//-----------------------------------------------------------------------------------------------------------

CylindricalDomain::CylindricalDomain(const Vector& center,
    const Float radius,
    const Float height,
    const bool includeBases)
    : center(center)
    , radius(radius)
    , height(height)
    , includeBases(includeBases) {}

Vector CylindricalDomain::getCenter() const {
    return center;
}

Float CylindricalDomain::getVolume() const {
    return PI * sqr(radius) * height;
}

Box CylindricalDomain::getBoundingBox() const {
    const Vector sides(radius, radius, 0.5_f * height);
    return Box(this->center - sides, this->center + sides);
}

bool CylindricalDomain::contains(const Vector& v) const {
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
            v = getNormalized(Vector(v[X], v[Y], this->center[Z]) - this->center) * (1._f - EPS) * radius +
                Vector(this->center[X], this->center[Y], v[Z]);
            if (includeBases) {
                const Float halfHeight = 0.5_f * (1._f - EPS) * height;
                v[Z] = clamp(v[Z], this->center[Z] - halfHeight, this->center[Z] + halfHeight);
            }
            v[H] = h;
            ASSERT(isInsideImpl(v));
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

//-----------------------------------------------------------------------------------------------------------
// HexagonalDomain implementation
//-----------------------------------------------------------------------------------------------------------

HexagonalDomain::HexagonalDomain(const Vector& center,
    const Float radius,
    const Float height,
    const bool includeBases)
    : center(center)
    , outerRadius(radius)
    , innerRadius(sqrt(0.75_f) * outerRadius)
    , height(height)
    , includeBases(includeBases) {}

Vector HexagonalDomain::getCenter() const {
    return center;
}

Float HexagonalDomain::getVolume() const {
    // 6 equilateral triangles
    return 1.5_f * sqrt(3._f) * sqr(outerRadius);
}

Box HexagonalDomain::getBoundingBox() const {
    const Vector sides(outerRadius, outerRadius, 0.5_f * height);
    return Box(this->center - sides, this->center + sides);
}

bool HexagonalDomain::contains(const Vector& v) const {
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
            const Float r = (1._f - EPS) * outerRadius * hexagon(phi);
            const Vector u = r * getNormalized(Vector(v[X], v[Y], 0._f));
            v = Vector(u[X], u[Y], v[Z], v[H]);
            if (includeBases) {
                const Float halfHeight = 0.5_f * (1._f - EPS) * height;
                v[Z] = clamp(v[Z], this->center[Z] - halfHeight, this->center[Z] + halfHeight);
            }
            ASSERT(isInsideImpl(v));
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

//-----------------------------------------------------------------------------------------------------------
// HalfSpaceDomain implementation
//-----------------------------------------------------------------------------------------------------------

Vector HalfSpaceDomain::getCenter() const {
    // z == 0, x and y are arbitrary
    return Vector(0._f);
}

Float HalfSpaceDomain::getVolume() const {
    return INFTY;
}

Box HalfSpaceDomain::getBoundingBox() const {
    return Box(Vector(-INFTY, -INFTY, 0._f), Vector(INFTY));
}

bool HalfSpaceDomain::contains(const Vector& v) const {
    return v[Z] > 0._f;
}

void HalfSpaceDomain::getSubset(ArrayView<const Vector> vs,
    Array<Size>& output,
    const SubsetType type) const {
    switch (type) {
    case SubsetType::OUTSIDE:
        for (Size i = 0; i < vs.size(); ++i) {
            if (!this->contains(vs[i])) {
                output.push(i);
            }
        }
        break;
    case SubsetType::INSIDE:
        for (Size i = 0; i < vs.size(); ++i) {
            if (this->contains(vs[i])) {
                output.push(i);
            }
        }
    }
}

void HalfSpaceDomain::getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const {
    distances.clear();
    for (const Vector& v : vs) {
        distances.push(v[Z]);
    }
}

void HalfSpaceDomain::project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices) const {
    if (indices) {
        for (Size i : indices.value()) {
            if (!this->contains(vs[i])) {
                vs[i][Z] = 0._f;
            }
        }
    } else {
        for (Size i = 0; i < vs.size(); ++i) {
            if (!this->contains(vs[i])) {
                vs[i][Z] = 0._f;
            }
        }
    }
}

void HalfSpaceDomain::addGhosts(ArrayView<const Vector> vs,
    Array<Ghost>& ghosts,
    const Float eta,
    const Float eps) const {
    ASSERT(eps < eta);
    ghosts.clear();
    for (Size i = 0; i < vs.size(); ++i) {
        if (!this->contains(vs[i])) {
            continue;
        }

        const Float dist = vs[i][Z];
        ASSERT(dist > 0._f);
        if (dist < vs[i][H] * eta) {
            Vector g = vs[i];
            g[Z] -= 2._f * dist;
            ghosts.push(Ghost{ g, i });
        }
    }
}

//-----------------------------------------------------------------------------------------------------------
// TransformedDomain implementation
//-----------------------------------------------------------------------------------------------------------

TransformedDomain::TransformedDomain(SharedPtr<IDomain> domain, const AffineMatrix& matrix)
    : domain(std::move(domain)) {
    tm = matrix;
    tmInv = matrix.inverse();
}

Vector TransformedDomain::getCenter() const {
    return domain->getCenter() + tm.translation();
}

Float TransformedDomain::getVolume() const {
    return domain->getVolume() * tm.determinant();
}

Box TransformedDomain::getBoundingBox() const {
    const Box box = domain->getBoundingBox();
    // transform all 8 points
    Box transformedBox;
    for (Size i = 0; i <= 1; ++i) {
        for (Size j = 0; j <= 1; ++j) {
            for (Size k = 0; k <= 1; ++k) {
                const Vector p = box.lower() * Vector(i, j, k) + box.upper() * Vector(1 - i, 1 - j, 1 - k);
                transformedBox.extend(tm * p);
            }
        }
    }
    return transformedBox;
}

bool TransformedDomain::contains(const Vector& v) const {
    return domain->contains(tmInv * v);
}

void TransformedDomain::getSubset(ArrayView<const Vector> vs,
    Array<Size>& output,
    const SubsetType type) const {
    domain->getSubset(this->untransform(vs), output, type);
}

void TransformedDomain::getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const {
    return domain->getDistanceToBoundary(this->untransform(vs), distances);
}

void TransformedDomain::project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices) const {
    return domain->project(this->untransform(vs), indices);
}

void TransformedDomain::addGhosts(ArrayView<const Vector> vs,
    Array<Ghost>& ghosts,
    const Float eta,
    const Float eps) const {
    domain->addGhosts(this->untransform(vs), ghosts, eta, eps);
    for (Ghost& g : ghosts) {
        g.position = tm * g.position;
    }
}

Array<Vector> TransformedDomain::untransform(ArrayView<const Vector> vs) const {
    Array<Vector> untransformed(vs.size());
    for (Size i = 0; i < vs.size(); ++i) {
        untransformed[i] = tmInv * vs[i];
    }
    return untransformed;
}


NAMESPACE_SPH_END
