#include "objects/geometry/Delaunay.h"
#include "io/Logger.h"
#include "math/AffineMatrix.h"
#include "math/Morton.h"
#include "math/rng/VectorRng.h"
#include "objects/geometry/Plane.h"
#include "objects/utility/Algorithm.h"
#include "objects/utility/IteratorAdapters.h"
#include "objects/utility/OutputIterators.h"

NAMESPACE_SPH_BEGIN

// ----------------------------------------------------------------------------------------------------------
// Tetrahedron
// ----------------------------------------------------------------------------------------------------------

Tetrahedron::Tetrahedron(const Vector& v1, const Vector& v2, const Vector& v3, const Vector& v4) {
    vertices[0] = v1;
    vertices[1] = v2;
    vertices[2] = v3;
    vertices[3] = v4;
}

Tetrahedron::Tetrahedron(const Triangle& tri, const Vector& v)
    : Tetrahedron(tri[0], tri[1], tri[2], v) {}

Tetrahedron::Tetrahedron(const StaticArray<Vector, 4>& vertices)
    : vertices(vertices.clone()) {}

Triangle Tetrahedron::triangle(const Size fi) const {
    switch (fi) {
    case 0:
        return { vertex(1), vertex(2), vertex(3) };
    case 1:
        return { vertex(0), vertex(3), vertex(2) };
    case 2:
        return { vertex(0), vertex(1), vertex(3) };
    case 3:
        return { vertex(0), vertex(2), vertex(1) };
    default:
        STOP;
    }
}

Float Tetrahedron::signedVolume() const {
    const Vector v1 = vertices[1] - vertices[0];
    const Vector v2 = vertices[2] - vertices[0];
    const Vector v3 = vertices[3] - vertices[0];
    return dot(v1, cross(v2, v3)) / 6._f;
}

Float Tetrahedron::volume() const {
    return abs(signedVolume());
}

Vector Tetrahedron::center() const {
    return (vertices[0] + vertices[1] + vertices[2] + vertices[3]) / 4._f;
}

Optional<Sphere> Tetrahedron::circumsphere() const {
    const Optional<Vector> center = this->circumcenter();
    if (!center) {
        return NOTHING;
    }
    const Float radius = getLength(vertices[0] - center.value());
    SPH_ASSERT(radius < LARGE);

#ifdef SPH_DEBUG
    for (Size i = 1; i < 4; ++i) {
        const Float altRadius = getLength(vertices[i] - center.value());
        SPH_ASSERT(almostEqual(radius, altRadius, 1.e-4_f), radius, altRadius);
    }
#endif
    const Sphere sphere(center.value(), radius);
    return sphere;
}

bool Tetrahedron::contains(const Vector& p) const {
    SPH_ASSERT(this->signedVolume() >= 0._f);
    for (Size fi = 0; fi < 4; ++fi) {
        if (Plane(triangle(fi)).above(p)) {
            return false;
        }
    }
    return true;
}

Tetrahedron Tetrahedron::unit() {
    return Tetrahedron(Vector(sqrt(8._f / 9._f), 0, -1._f / 3._f),
        Vector(-sqrt(2._f / 9._f), sqrt(2._f / 3._f), -1._f / 3._f),
        Vector(-sqrt(2._f / 9._f), -sqrt(2._f / 3._f), -1._f / 3._f),
        Vector(0, 0, 1));
}

Optional<Vector> Tetrahedron::circumcenter() const {
    const Vector d1 = vertices[1] - vertices[0];
    const Vector d2 = vertices[2] - vertices[0];
    const Vector d3 = vertices[3] - vertices[0];
    AffineMatrix A(d1, d2, d3);
    SPH_ASSERT(A.translation() == Vector(0._f));
    Optional<AffineMatrix> A_inv = A.tryInverse();
    if (!A_inv) {
        return NOTHING;
    }

    const Vector B = 0.5_f * Vector(getSqrLength(d1), getSqrLength(d2), getSqrLength(d3));
    return A_inv.value() * B + vertices[0];
}

// ----------------------------------------------------------------------------------------------------------
// utility functions
// ----------------------------------------------------------------------------------------------------------

using Face = Delaunay::Face;
using Cell = Delaunay::Cell;

INLINE Face toKey(const Face& f) {
    if (f[0] < f[1] && f[0] < f[2]) {
        return f;
    } else if (f[1] < f[0] && f[1] < f[2]) {
        return Face(f[1], f[2], f[0]);
    } else {
        return Face(f[2], f[0], f[1]);
    }
}

INLINE bool isSuper(const Face& f) {
    return f[0] < 4 && f[1] < 4 && f[2] < 4;
}

inline bool opposite(const Face& f1, const Face& f2) {
    for (Size i1 = 0; i1 < 3; ++i1) {
        if (f1[i1] == f2[0]) {
            const Size i2 = (i1 + 1) % 3;
            const Size i3 = (i1 + 2) % 3;
            if (f1[i2] == f2[2] && f1[i3] == f2[1]) {
                return true;
            }
        }
    }
    return false;
}

static Optional<Size> intersect(const Tetrahedron& tet,
    const Vector& origin,
    const Vector& target,
    const Vector& dir) {
    Size fi_min = Size(-1);
    Float t_min = INFTY;
    for (Size fi1 = 0; fi1 < 4; ++fi1) {
        const Plane plane(tet.triangle(fi1));
        const Vector is = plane.intersection(origin, dir);

        bool contains = true;
        for (Size fi2 = 0; fi2 < 4; ++fi2) {
            if (fi1 == fi2) {
                continue;
            }
            if (Plane(tet.triangle(fi2)).above(is)) {
                contains = false;
                break;
            }
        }

        if (contains) {
            const Float t = getSqrLength(is - target);
            if (t < t_min) {
                t_min = t;
                fi_min = fi1;
            }
        }
    }
    if (t_min < INFTY) {
        SPH_ASSERT(fi_min != Size(-1));
        return fi_min;
    } else {
        return NOTHING;
    }
}


// ----------------------------------------------------------------------------------------------------------
// Delaunay::Cell
// ----------------------------------------------------------------------------------------------------------

Delaunay::Cell::Cell(const Size a, const Size b, const Size c, const Size d, const Sphere& sphere)
    : idxs{ a, b, c, d }
    , sphere(sphere) {
#ifdef SPH_DEBUG
    SPH_ASSERT(allUnique({ a, b, c, d }));
    for (Size fi = 0; fi < 4; ++fi) {
        SPH_ASSERT(neighs[fi].handle == nullptr);
    }
#endif
}

Face Delaunay::Cell::face(const Size fi) const {
    switch (fi) {
    case 0:
        return Face(idxs[1], idxs[2], idxs[3]);
    case 1:
        return Face(idxs[0], idxs[3], idxs[2]);
    case 2:
        return Face(idxs[0], idxs[1], idxs[3]);
    case 3:
        return Face(idxs[0], idxs[2], idxs[1]);
    default:
        STOP;
    }
}

Delaunay::Cell::Handle Delaunay::Cell::neighbor(const Size fi) const {
    return neighs[fi].handle;
}

void Delaunay::Cell::detach() {
    for (Size fi1 = 0; fi1 < 4; ++fi1) {
        Handle nh = neighs[fi1].handle;
        if (!nh) {
            continue;
        }
        const Size fi2 = neighs[fi1].mirror;
        SPH_ASSERT(nh->neighbor(fi2) == this, nh->neighbor(fi2), this);

        nh->setNeighbor(fi2, nullptr, Size(-1));
        neighs[fi1].handle = nullptr;
        neighs[fi1].mirror = Size(-1);
    }
    SPH_ASSERT(isDetached());
}

void Delaunay::Cell::setNeighbor(const Size fi, const Handle& ch, const Size mirror) {
#ifdef SPH_DEBUG
    SPH_ASSERT(fi < 4);
    SPH_ASSERT(mirror < 4 || (ch == nullptr && mirror == Size(-1)));
    // no two faces can share a neighbors
    for (Size fi2 = 0; fi2 < 4; ++fi2) {
        if (fi2 == fi) {
            continue;
        }
        Handle nch = neighs[fi2].handle;
        SPH_ASSERT(!nch || nch != ch);
    }
#endif
    neighs[fi].handle = ch;
    neighs[fi].mirror = mirror;
}

Size Delaunay::Cell::getNeighborCnt() const {
    Size cnt = 0;
    for (Size i = 0; i < 4; ++i) {
        if (neighs[i].handle != nullptr) {
            cnt++;
        }
    }
    return cnt;
}

Size Delaunay::Cell::mirror(const Size fi) const {
    SPH_ASSERT(neighs[fi].handle != nullptr);
    SPH_ASSERT(neighs[fi].mirror != Size(-1));
    return neighs[fi].mirror;
}

// ----------------------------------------------------------------------------------------------------------
// Delaunay implementation
// ----------------------------------------------------------------------------------------------------------

Delaunay::Delaunay(const std::size_t allocatorMemory)
    : resource(allocatorMemory, alignof(Cell)) {
    allocator.primary().bind(resource);

#ifdef SPH_DEBUG
    logger = makeAuto<StdOutLogger>();
#else
    logger = makeAuto<NullLogger>();
#endif
}

Delaunay::~Delaunay() {
    for (Cell::Handle ch : cells) {
        ch->detach();
        allocatorDelete(allocator, ch);
    }
}

struct DegenerateTetrahedron {};

void Delaunay::build(ArrayView<const Vector> points, const Flags<BuildFlag> flags) {
    try {
        if (flags.has(BuildFlag::SPATIAL_SORT)) {
            Array<Vector> sortedPoints;
            sortedPoints.pushAll(points.begin(), points.end());
            spatialSort(sortedPoints);

            this->buildImpl(sortedPoints);
        } else {
            this->buildImpl(points);
        }
    } catch (const DegenerateTetrahedron&) {
        // perturb the input points and retry
        Array<Vector> perturbedPoints;
        perturbedPoints.pushAll(points.begin(), points.end());

        Box box;
        for (const Vector& p : points) {
            box.extend(p);
        }

        VectorRng<UniformRng> rng;
        const Float magnitude = 1.e-8_f * maxElement(box.size());
        for (Size i = 0; i < perturbedPoints.size(); ++i) {
            perturbedPoints[i] += magnitude * (2._f * rng() - Vector(1._f));
        }
        this->build(perturbedPoints, flags);
    }
}

void Delaunay::buildImpl(ArrayView<const Vector> points) {
    vertices.clear();
    vertices.reserve(points.size() + 4);
    cells.clear();

    Box box;
    for (const Vector& p : points) {
        box.extend(p);
    }
    const Vector center = box.center();
    const Float side = 4._f * maxElement(box.size());
    Tetrahedron super = Tetrahedron::unit();
    for (Size i = 0; i < 4; ++i) {
        super.vertex(i) = super.vertex(i) * side + center;
        vertices.push(super.vertex(i));
    }
    Cell::Handle root = allocatorNew<Cell>(allocator, 0, 1, 2, 3, super.circumsphere().value());
    cellCnt = 1;

    SPH_ASSERT(tetrahedron(*root).contains(super.center()));
    SPH_ASSERT(tetrahedron(*root).signedVolume() > 0._f);

#ifdef SPH_DEBUG
    const Float volume0 = tetrahedron(*root).volume();
    for (const Vector& p : points) {
        SPH_ASSERT(super.contains(p), p);
        SPH_ASSERT(root->circumsphere().contains(p));
    }
#endif

    Cell::Handle hint = root;
    this->startProgress(points.size());
    for (const Vector& p : points) {
        hint = this->addPoint(p, hint);
        this->tickProgress();
    }

    // root is already deleted at this point!!

    cells.reserve(cellCnt);
    this->region(hint, backInserter(cells), [](const Cell& UNUSED(c)) { return true; });
    SPH_ASSERT(cellCnt == cells.size());

#ifdef SPH_DEBUG
    Float volume = 0._f;
    for (Cell::Handle ch : cells) {
        volume += tetrahedron(*ch).volume();
    }
    SPH_ASSERT(almostEqual(volume, volume0), volume, volume0);
#endif

    /// \todo optimize - do not insert them in the first place
    for (Size ci = 0; ci < cells.size();) {
        const Cell& c = *cells[ci];
        if (c[0] < 4 || c[1] < 4 || c[2] < 4 || c[3] < 4) {
            cells[ci]->detach();
            allocatorDelete(allocator, cells[ci]);
            std::swap(cells[ci], cells.back());
            cells.pop();
        } else {
            ++ci;
        }
    }
}


Array<Triangle> Delaunay::convexHull() const {
    return surface([](const Cell& UNUSED(c)) { return true; });
}

Array<Triangle> Delaunay::alphaShape(const Float alpha) const {
    const Float alphaSqr = sqr(alpha);
    return surface([this, alphaSqr](const Cell& c) {
        const Vector v1 = vertices[c[0]];
        const Vector v2 = vertices[c[1]];
        const Vector v3 = vertices[c[2]];
        const Vector v4 = vertices[c[3]];

        const Float e12 = getSqrLength(v1 - v2);
        const Float e13 = getSqrLength(v1 - v3);
        const Float e14 = getSqrLength(v1 - v4);
        const Float e23 = getSqrLength(v2 - v3);
        const Float e24 = getSqrLength(v2 - v4);
        const Float e34 = getSqrLength(v3 - v4);
        return max(e12, e13, e14, e23, e24, e34) < alphaSqr;
    });
}

Cell::Handle Delaunay::locate(const Vector& p, const Cell::Handle hint) const {
    Cell::Handle seed;
    if (hint) {
        seed = hint;
    } else {
        seed = cells.front();
    }
    return this->locate(p, seed, [this](const Cell& c, const Vector& p) { //
        return tetrahedron(c).contains(p);
    });
}

Delaunay::Cell::Handle Delaunay::addPoint(const Vector& p, const Cell::Handle hint) {
    // implements the Bowyer–Watson algorithm
    vertices.push(p);

    const Cell::Handle seed = this->locate(p, hint, [](const Cell& c, const Vector& p) { //
        return c.circumsphere().contains(p);
    });

    badSet.clear();
    this->region(seed, backInserter(badSet), [&p](const Cell& c) { //
        return c.circumsphere().contains(p);
    });

    added.clear();
    Cell::Handle nextHint = nullptr;
    for (const Cell::Handle ch : badSet) {
        SPH_ASSERT(ch->circumsphere().contains(p));
        for (Size fi = 0; fi < 4; ++fi) {
            const Cell::Handle nh = ch->neighbor(fi);
            if (nh && contains(badSet, nh)) {
                continue;
            }

            nextHint = this->triangulate(ch, fi, p);
        }
    }

    cellCnt -= badSet.size();
    for (Cell::Handle ch : badSet) {
        ch->detach();
    }

    this->updateConnectivity();

    for (Cell::Handle ch : badSet) {
        allocatorDelete(allocator, ch);
    }

    return nextHint;
}

Delaunay::Cell::Handle Delaunay::triangulate(const Cell::Handle ch1, const Size fi1, const Vector& p) {
    Face f1 = ch1->face(fi1);
    Cell::Handle ch2;
    Tetrahedron tet(triangle(f1), p);
    Optional<Sphere> sphere = tet.circumsphere();
    if (!sphere) {
        logger->write("Degenerate tetrahedron!");
        throw DegenerateTetrahedron();
    }
    ch2 = allocatorNew<Cell>(allocator, f1[0], f1[2], f1[1], vertices.size() - 1, sphere.value());

#ifdef SPH_DEBUG
    const Tetrahedron tet2 = tetrahedron(*ch2);
    SPH_ASSERT(tet2.signedVolume() > 0, tet2.signedVolume());
    SPH_ASSERT(tet2.contains(tet.center()));
#endif

    const Face f2 = ch2->face(3);

    // fix connectivity
    if (const Cell::Handle nch1 = ch1->neighbor(fi1)) {
        SPH_ASSERT(ch1 && !contains(badSet, nch1));
        const Size nfi1 = ch1->mirror(fi1);
        // detach the neighbor thats now connected to ch2
        ch1->setNeighbor(fi1, nullptr, Size(-1));
        this->setNeighbors(nch1, nfi1, ch2, 3);
    } else {
        /// \todo avoid having to detach later ...
        // ch1->detach();
        SPH_ASSERT(isSuper(f2), f2[0], f2[1], f2[2]);
    }

    // last face is already connected
    for (Size i = 0; i < 3; ++i) {
        added.push(makeTuple(ch2, i, toKey(ch2->face(i))));
    }
    cellCnt++;

    return ch2;
}

void Delaunay::updateConnectivity() const {
    Cell::Handle ch1, ch2;
    Size fi1, fi2;
    Face f1, f2;
    for (Size i1 = 0; i1 < added.size(); ++i1) {
        tieToTuple(ch1, fi1, f1) = added[i1];
        for (Size i2 = i1 + 1; i2 < added.size(); ++i2) {
            tieToTuple(ch2, fi2, f2) = added[i2];
            if (f1 == f2.opposite()) {
                SPH_ASSERT(!contains(badSet, ch1));
                SPH_ASSERT(!contains(badSet, ch2));

                SPH_ASSERT(opposite(ch1->face(fi1), ch2->face(fi2)));
                this->setNeighbors(ch1, fi1, ch2, fi2);
                break;
            }
        }
    }
}

void Delaunay::setNeighbors(const Cell::Handle ch1,
    const Size fi1,
    const Cell::Handle ch2,
    const Size fi2) const {
    SPH_ASSERT(!ch1 || !ch2 || opposite(ch1->face(fi1), ch2->face(fi2)));
    if (ch1) {
        SPH_ASSERT(bool(ch2) == (fi2 != Size(-1)));
        ch1->setNeighbor(fi1, ch2, fi2);
    }
    if (ch2) {
        SPH_ASSERT(bool(ch1) == (fi1 != Size(-1)));
        ch2->setNeighbor(fi2, ch1, fi1);
    }
}

template <typename TInsideFunc>
Cell::Handle Delaunay::locate(const Vector& p, const Cell::Handle seed, const TInsideFunc& inside) const {
    SPH_ASSERT(seed != nullptr);
    Cell::Handle ch = seed;

    const Vector from = tetrahedron(*ch).center();
    const Vector dir = getNormalized(p - from);
    SPH_ASSERT(tetrahedron(*ch).contains(from));
    SPH_ASSERT(inside(*ch, from));

    while (!inside(*ch, p)) {
        Optional<Size> nextFi = intersect(tetrahedron(*ch), from, p, dir);
        SPH_ASSERT(nextFi);

        ch = ch->neighbor(nextFi.value());
        SPH_ASSERT(ch);
    }
    return ch;
}

template <typename TOutIter, typename TPredicate>
void Delaunay::region(const Cell::Handle seed, TOutIter out, const TPredicate& predicate) {
    stack.clear();
    visited.clear();
    stack.push(seed);

    seed->setVisited(true);

    while (!stack.empty()) {
        Cell::Handle ch = stack.pop();
        *out = ch;
        ++out;

        for (Size fi = 0; fi < 4; ++fi) {
            Cell::Handle nch = ch->neighbor(fi);
            if (!nch || nch->visited()) {
                continue;
            }
            nch->setVisited(true);
            visited.push(nch);
            if (predicate(*nch)) {
                stack.push(nch);
            }
        }
    }

    // clear the visited flag
    for (Cell::Handle ch : visited) {
        ch->setVisited(false);
    }
}

template <typename TInsideFunc>
Array<Triangle> Delaunay::surface(const TInsideFunc& func) const {
    Array<Triangle> triangles;
    for (const Cell::Handle ch : cells) {
        if (!func(*ch)) {
            continue;
        }
        for (Size fi = 0; fi < 4; ++fi) {
            Cell::Handle nch = ch->neighbor(fi);
            if (nch && func(*nch)) {
                continue;
            }

            Triangle tr = triangle(ch->face(fi));
            triangles.push(tr);
        }
    }
    return triangles;
}

NAMESPACE_SPH_END
