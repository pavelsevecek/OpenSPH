#pragma once

#include "math/AffineMatrix.h"
#include "math/Morton.h"
#include "objects/Exceptions.h"
#include "objects/containers/AdvancedAllocators.h"
#include "objects/containers/FlatMap.h"
#include "objects/containers/FlatSet.h"
#include "objects/containers/UnorderedMap.h"
#include "objects/geometry/Plane.h"
#include "objects/geometry/Sphere.h"
#include "objects/utility/IteratorAdapters.h"
#include "objects/utility/OutputIterators.h"
#include "objects/wrappers/SharedPtr.h"
#include "system/Timer.h"
#include <iostream>
#include <map>
#include <set>
#include <unordered_set>

NAMESPACE_SPH_BEGIN

class Tetrahedron {
private:
    StaticArray<Vector, 4> vertices;

public:
    Tetrahedron() = default;

    Tetrahedron(const Vector& v1, const Vector& v2, const Vector& v3, const Vector& v4) {
        vertices[0] = v1;
        vertices[1] = v2;
        vertices[2] = v3;
        vertices[3] = v4;
    }

    Tetrahedron(const Triangle& tri, const Vector& v)
        : Tetrahedron(tri[0], tri[1], tri[2], v) {}

    Tetrahedron(const StaticArray<Vector, 4>& vertices)
        : vertices(vertices.clone()) {}

    Vector& vertex(const Size i) {
        return vertices[i];
    }

    const Vector& vertex(const Size i) const {
        return vertices[i];
    }

    Triangle triangle(const Size fi) const {
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

    Float signedVolume() const {
        const Vector v1 = vertices[1] - vertices[0];
        const Vector v2 = vertices[2] - vertices[0];
        const Vector v3 = vertices[3] - vertices[0];
        return dot(v1, cross(v2, v3)) / 6._f;
    }

    Float volume() const {
        return abs(signedVolume());
    }

    Vector center() const {
        return (vertices[0] + vertices[1] + vertices[2] + vertices[3]) / 4._f;
    }

    Optional<Sphere> circumsphere() const {
        const Optional<Vector> center = this->circumcenter();
        if (!center) {
            return NOTHING;
        }
        const Float radius = getLength(vertices[0] - center.value());
        SPH_ASSERT(radius < LARGE);

#ifdef SPH_DEBUG
        for (Size i = 1; i < 4; ++i) {
            const Float altRadius = getLength(vertices[i] - center.value());
            SPH_ASSERT(almostEqual(radius, altRadius, 1.e-8_f), radius, altRadius);
        }
#endif
        const Sphere sphere(center.value(), radius);
        return sphere;
    }

    bool contains(const Vector& p) const {
        for (Size fi = 0; fi < 4; ++fi) {
            if (Plane(triangle(fi)).above(p)) {
                return false;
            }
        }
        return true;
    }

    static Tetrahedron unit() {
        return Tetrahedron(Vector(sqrt(8._f / 9._f), 0, -1._f / 3._f),
            Vector(-sqrt(2._f / 9._f), sqrt(2._f / 3._f), -1._f / 3._f),
            Vector(-sqrt(2._f / 9._f), -sqrt(2._f / 3._f), -1._f / 3._f),
            Vector(0, 0, 1));
    }

private:
    Optional<Vector> circumcenter() const {
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
};

class Delaunay {
private:
    Array<Vector> vertices;

    class Face {
    private:
        Size idxs[3];

    public:
        Face() = default;
        Face(const Size a, const Size b, const Size c) {
            idxs[0] = a;
            idxs[1] = b;
            idxs[2] = c;
        }


        Size operator[](const Size vi) const {
            SPH_ASSERT(vi < 4);
            return idxs[vi];
        }

        Size& operator[](const Size vi) {
            SPH_ASSERT(vi < 4);
            return idxs[vi];
        }

        Face opposite() const {
            return Face(idxs[0], idxs[2], idxs[1]);
        }

        bool operator==(const Face& other) const {
            return idxs[0] == other.idxs[0] && idxs[1] == other.idxs[1] && idxs[2] == other.idxs[2];
        }

        bool operator<(const Face& other) const {
            return makeTuple(idxs[0], idxs[1], idxs[2]) <
                   makeTuple(other.idxs[0], other.idxs[1], other.idxs[2]);
        }
    };

    class Cell {
    public:
        using Handle = Cell*;

    private:
        Size idxs[4];

        struct Neigh {
            Handle handle = nullptr;
            Size mirror = Size(-1);
        };

        Neigh neighs[4];

        Sphere sphere;
        bool flag = false;

    public:
        Cell() = default;

        Cell(const Size a, const Size b, const Size c, const Size d, const Sphere& sphere)
            : idxs{ a, b, c, d }
            , sphere(sphere) {
#ifdef SPH_DEBUG
            StaticArray<Size, 4> sortedIdxs({ a, b, c, d });
            std::sort(sortedIdxs.begin(), sortedIdxs.end());
            SPH_ASSERT(std::unique(sortedIdxs.begin(), sortedIdxs.end()) == sortedIdxs.end());

            for (Size fi = 0; fi < 4; ++fi) {
                SPH_ASSERT(neighs[fi].handle == nullptr);
            }
#endif
        }

        Cell(const Cell& other) = delete;

        ~Cell() {
            SPH_ASSERT(this->isDetached());
        }

        Cell& operator=(const Cell& other) = delete;

        Size operator[](const Size vi) const {
            SPH_ASSERT(vi < 4);
            return idxs[vi];
        }

        Size& operator[](const Size vi) {
            SPH_ASSERT(vi < 4);
            return idxs[vi];
        }

        Handle neighbor(const Size fi) const {
            return neighs[fi].handle;
        }

        void setNeighbor(const Size fi, const Handle& ch, const Size mirror) {
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

        Size neighborCnt() const {
            Size cnt = 0;
            for (Size i = 0; i < 4; ++i) {
                if (neighs[i].handle != nullptr) {
                    cnt++;
                }
            }
            return cnt;
        }

        Size mirror(const Size fi) const {
            SPH_ASSERT(neighs[fi].handle != nullptr);
            SPH_ASSERT(neighs[fi].mirror != Size(-1));
            return neighs[fi].mirror;
        }

        void detach() {
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

        bool isDetached() const {
            return neighborCnt() == 0;
        }

        const Sphere& circumsphere() const {
            return sphere;
        }

        bool visited() const {
            return flag;
        }

        void setVisited(bool value) {
            flag = value;
        }
    };

    Array<Cell::Handle> cells;
    Size cellCnt = 0;

    // using Polyhedron = Array<std::pair<Cell::Handle, Size>>;


    static Face toKey(const Face& f) {
        if (f[0] < f[1] && f[0] < f[2]) {
            return f;
        } else if (f[1] < f[0] && f[1] < f[2]) {
            return Face(f[1], f[2], f[0]);
        } else {
            return Face(f[2], f[0], f[1]);
        }
    }

    static bool isKey(const Face& f) {
        return f[0] < f[1] && f[0] < f[2];
    }

    static bool isSuper(const Face& f) {
        return f[0] < 4 && f[1] < 4 && f[2] < 4;
    }

    // FlatMap<Face, Tuple<Cell::Handle, Size>> invalidated;
    // UnorderedMap<Face, Tuple<Cell::Handle, Size>> added;
    Array<Tuple<Cell::Handle, Size, Face>> added;

    Array<Cell::Handle> stack;
    Array<Cell::Handle> visited;
    Array<Cell::Handle> badSet;

    using Resource = MonotonicMemoryResource<Mallocator>;
    // using Allocator = Mallocator;
    using Allocator = FallbackAllocator<MemoryResourceAllocator<Resource>, Mallocator>;
    // using Allocator = FreeListAllocator<FallbackAllocator<MemoryResourceAllocator<Resource>, Mallocator>>;

    Resource resource = Resource(1ull << 30, 1);
    Allocator allocator;

public:
    Delaunay() {
        allocator.primary().bind(resource);
        //        allocator.underlying().primary().bind(resource);
    }

    ~Delaunay() {
        for (Cell::Handle ch : cells) {
            ch->detach();
            allocatorDelete(allocator, ch);
        }
    }

    void build(ArrayView<const Vector> points) {
        vertices.clear();
        vertices.reserve(points.size() + 4);
        cells.clear();

        Array<Vector> sortedPoints;
        sortedPoints.insert(0, points.begin(), points.end());
        // std::random_shuffle(sortedPoints.begin(), sortedPoints.end());
        spatialSort(sortedPoints);

        Box box;
        for (const Vector& p : sortedPoints) {
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
        // cells.insert(ch);
        SPH_ASSERT(tetrahedron(*root).contains(super.center()));
        SPH_ASSERT(tetrahedron(*root).signedVolume() > 0._f);

#ifdef SPH_DEBUG
        const Float volume0 = tetrahedron(*root).volume();
        std::cout << "Volume = " << volume0 << std::endl;
        for (const Vector& p : sortedPoints) {
            SPH_ASSERT(super.contains(p), p);
            SPH_ASSERT(root->circumsphere().contains(p));
        }
#endif

        Size index = 0;
        Cell::Handle hint = root;
        for (const Vector& p : sortedPoints) {
            index++;
            if (index % 100 == 0) {
                std::cout << "Adding point " << index << " out of " << sortedPoints.size() << std::endl;
            }
            hint = this->addPoint(p, hint);
        }

        // root is already deleted at this point!!

        std::cout << "Getting the list of cells" << std::endl;
        cells.reserve(cellCnt);
        Timer timer;
        this->region(hint, backInserter(cells), [](const Cell& UNUSED(c)) { return true; });
        std::cout << "Region took " << timer.elapsed(TimerUnit::MILLISECOND) << " ms" << std::endl;
        std::cout << "Got " << cells.size() << " cells" << std::endl;
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

    Tetrahedron tetrahedron(const Cell& c) const {
        return Tetrahedron(vertices[c[0]], vertices[c[1]], vertices[c[2]], vertices[c[3]]);
    }

    Size getTetrahedraCnt() const {
        return cells.size();
    }

    bool insideCell(const Cell& c, const Vector& p) const {
        for (Size fi = 0; fi < 4; ++fi) {
            const Face& f = face(c, fi);
            const Plane plane(triangle(f));
            if (plane.signedDistance(p) > 0._f) {
                return false;
            }
        }
        return true;
    }

    Array<Triangle> convexHull() const {
        return surface([](const Cell& UNUSED(c)) { return true; });
    }

    Array<Triangle> alphaShape(const Float alpha) const {
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

private:
    NO_INLINE Cell::Handle addPoint(const Vector& p, const Cell::Handle& hint) {
        // implements the Bowyer–Watson algorithm

        vertices.push(p);

        const Cell::Handle seed = this->locate(p, hint, [](const Cell& c, const Vector& p) { //
            return c.circumsphere().contains(p);
        });

        badSet.clear();
        this->region(seed, backInserter(badSet), [this, &p](const Cell& c) { //
            return c.circumsphere().contains(p);
        });

        added.clear();
        Cell::Handle nextHint = nullptr;
        for (const Cell::Handle& ch : badSet) {
            SPH_ASSERT(ch->circumsphere().contains(p));
            for (Size fi = 0; fi < 4; ++fi) {
                const Cell::Handle nh = ch->neighbor(fi);
                if (nh && std::find(badSet.begin(), badSet.end(), nh) != badSet.end()) { //.contains(nh)) {
                    continue;
                }

                nextHint = this->triangulate(ch, fi, p);
            }
        }

        cellCnt -= badSet.size();
        for (Cell::Handle ch : badSet) {
            ch->detach();
        }

        updateConnectivity();

        for (Cell::Handle ch : badSet) {
            allocatorDelete(allocator, ch);
        }

        return nextHint;
    }

    INLINE static Vector perturbDirection(const Vector& n) {
        // has to yield the same result for n and -n
        return Vector::unit(argMax(abs(n)));
    }

    NO_INLINE Cell::Handle triangulate(const Cell::Handle ch1, const Size fi1, const Vector& p) {
        Face f1 = face(*ch1, fi1);
        const Triangle tri = triangle(f1);
        const Plane plane(tri);
        Cell::Handle ch2;
        Tetrahedron tet(triangle(f1), p);
        Optional<Sphere> sphere = tet.circumsphere();
        Vector p_reg = p;
        if (!sphere) {
            // small perturbation
            const Float e10 = getSqrLength(tri[1] - tri[0]);
            const Float e20 = getSqrLength(tri[2] - tri[0]);
            const Float e21 = getSqrLength(tri[2] - tri[1]);
            const Float e = sqrt(max(e10, e20, e21));
            std::cout << "Denegerated tetrahedron!! Perturbing the added point by e=" << e << std::endl;
            p_reg += clearH(0.01_f * perturbDirection(plane.normal()) * e);
            vertices.back() = p_reg;
            tet = Tetrahedron(triangle(f1), p_reg);
            sphere = tet.circumsphere();
            SPH_ASSERT(sphere);
        }
        ch2 = allocatorNew<Cell>(allocator, f1[0], f1[2], f1[1], vertices.size() - 1, sphere.value());

        SPH_ASSERT(tetrahedron(*ch2).signedVolume() > 0);
        SPH_ASSERT(tetrahedron(*ch2).contains(tet.center()));

        const Face f2 = face(*ch2, 3);

        // fix connectivity
        if (const Cell::Handle nch1 = ch1->neighbor(fi1)) {
            // SPH_ASSERT(!badSet.contains(nch1));
            const Size nfi1 = ch1->mirror(fi1);
            // detach the neighbor thats now connected to ch2
            ch1->setNeighbor(fi1, nullptr, Size(-1));
            setNeighbor(nch1, nfi1, ch2, 3);
        } else {
            /// \todo avoid having to detach later ...
            // ch1->detach();
            SPH_ASSERT(isSuper(f2), f2[0], f2[1], f2[2]);
        }

        // last face is already connected
        for (Size i = 0; i < 3; ++i) {
            // added.insert(toKey(face(*ch2, i)), makeTuple(ch2, i));
            added.push(makeTuple(ch2, i, toKey(face(*ch2, i))));
        }
        cellCnt++;

        return ch2;
    }

    NO_INLINE void updateConnectivity() {
        Cell::Handle ch1, ch2;
        Size fi1, fi2;
        Face f1, f2;
        for (Size i1 = 0; i1 < added.size(); ++i1) {
            tieToTuple(ch1, fi1, f1) = added[i1];
            // const Face f1 = toKey(face(*ch1, fi1));
            for (Size i2 = i1 + 1; i2 < added.size(); ++i2) {
                /*if (i1 == i2) {
                    continue;
                }*/
                tieToTuple(ch2, fi2, f2) = added[i2];
                // const Face& f1 = a.key;
                // SPH_ASSERT(isKey(f1));

                // const Face f2 = toKey(f1.opposite());
                // SPH_ASSERT(isKey(f2));
                if (f1 == f2.opposite()) { // toKey(face(*ch2, fi2)).opposite()) {
                    SPH_ASSERT(std::find(badSet.begin(), badSet.end(), ch1) == badSet.end());
                    SPH_ASSERT(std::find(badSet.begin(), badSet.end(), ch2) == badSet.end());

                    SPH_ASSERT(opposite(face(*ch1, fi1), face(*ch2, fi2)));
                    setNeighbor(ch1, fi1, ch2, fi2);
                    break;
                }

                //    SPH_ASSERT(i2 != added.size() - 1);
            }
        }
    }

    void setNeighbor(const Cell::Handle& ch1, const Size fi1, const Cell::Handle& ch2, const Size fi2) {
        SPH_ASSERT(!ch1 || !ch2 || opposite(face(*ch1, fi1), face(*ch2, fi2)));
        if (ch1) {
            SPH_ASSERT(bool(ch2) == (fi2 != Size(-1)));
            ch1->setNeighbor(fi1, ch2, fi2);
        }
        if (ch2) {
            SPH_ASSERT(bool(ch1) == (fi1 != Size(-1)));
            ch2->setNeighbor(fi2, ch1, fi1);
        }
    }

    Face face(const Cell& c, const Size fi) const {
        switch (fi) {
        case 0:
            return Face(c[1], c[2], c[3]);
        case 1:
            return Face(c[0], c[3], c[2]);
        case 2:
            return Face(c[0], c[1], c[3]);
        case 3:
            return Face(c[0], c[2], c[1]);
        default:
            STOP;
        }
    }

    bool opposite(const Face& f1, const Face& f2) const {
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

    Triangle triangle(const Face& f) const {
        return Triangle(vertices[f[0]], vertices[f[1]], vertices[f[2]]);
    }

    template <typename TInsideFunc>
    Array<Triangle> surface(const TInsideFunc& func) const {
        Array<Triangle> triangles;
        for (const Cell::Handle& ch : cells) {
            if (!func(*ch)) {
                continue;
            }
            for (Size fi = 0; fi < 4; ++fi) {
                Cell::Handle nh = ch->neighbor(fi);
                if (nh && func(*nh)) {
                    continue;
                }

                Triangle tr = triangle(face(*ch, fi));
                triangles.push(tr);
            }
        }
        return triangles;
    }

    template <typename TOutIter, typename TPredicate>
    void region(const Cell::Handle& seed, TOutIter out, const TPredicate& predicate) {
        stack.clear();
        visited.clear();
        stack.push(seed);

        seed->setVisited(true);
        // visited.insert(seed);

        while (!stack.empty()) {
            Cell::Handle ch = stack.pop();
            *out = ch;
            ++out;
            // SPH_ASSERT(!visited.contains(ch));
            // visited.insert(ch);

            for (Size fi = 0; fi < 4; ++fi) {
                Cell::Handle nh = ch->neighbor(fi);
                if (!nh || nh->visited()) {
                    continue;
                }
                nh->setVisited(true);
                visited.push(nh);
                if (predicate(*nh)) {
                    // visited.insert(nh);
                    stack.push(nh);
                }
            }
        }

        // clear the visited flag
        for (Cell::Handle ch : visited) {
            ch->setVisited(false);
        }
    }

    Cell::Handle locate(const Vector& p, const Cell::Handle& hint) const {
        return this->locate(p, hint, [this](const Cell& c, const Vector& p) { //
            return tetrahedron(c).contains(p);
        });
    }

    template <typename TInsideFunc>
    Cell::Handle locate(const Vector& p, const Cell::Handle& hint, const TInsideFunc& inside) const {
        Cell::Handle ch = hint;

        const Vector from = tetrahedron(*ch).center();
        const Vector dir = getNormalized(p - from);
        SPH_ASSERT(tetrahedron(*ch).contains(from));
        SPH_ASSERT(inside(*ch, from));

        while (!inside(*ch, p)) {
            Optional<Size> nextFi = this->intersect(tetrahedron(*ch), from, p, dir);
            SPH_ASSERT(nextFi);

            ch = ch->neighbor(nextFi.value());
            SPH_ASSERT(ch);
        }
        return ch;
    }

    Optional<Size> intersect(const Tetrahedron& tet,
        const Vector& origin,
        const Vector& target,
        const Vector& dir) const {
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
            //  std::cout << "Found intersection at distance " << getLength(dir) - t_max << std::endl;
            return fi_min;
        } else {
            return NOTHING;
        }
    }
};

NAMESPACE_SPH_END
