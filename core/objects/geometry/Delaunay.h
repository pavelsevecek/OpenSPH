#pragma once

#include "math/AffineMatrix.h"
#include "math/Morton.h"
#include "objects/Exceptions.h"
#include "objects/containers/FlatMap.h"
#include "objects/containers/FlatSet.h"
#include "objects/geometry/Plane.h"
#include "objects/geometry/Sphere.h"
#include "objects/utility/IteratorAdapters.h"
#include "objects/utility/OutputIterators.h"
#include "objects/wrappers/SharedPtr.h"
#include "system/Timer.h"
#include <iostream>
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
    };

    class Cell {
    public:
        using Handle = Cell*;

    private:
        Size idxs[4];
        Sphere sphere;

        Handle neighs[4];

    public:
        Cell() = default;

        Cell(const Size a, const Size b, const Size c, const Size d, const Sphere& sphere)
            : idxs{ a, b, c, d }
            , sphere(sphere)
            , neighs{ nullptr, nullptr, nullptr, nullptr } {
#ifdef SPH_DEBUG
            StaticArray<Size, 4> sortedIdxs({ a, b, c, d });
            std::sort(sortedIdxs.begin(), sortedIdxs.end());
            SPH_ASSERT(std::unique(sortedIdxs.begin(), sortedIdxs.end()) == sortedIdxs.end());
#endif
        }

        Cell(const Cell& other) = default;

        ~Cell() {
            SPH_ASSERT(this->isDetached());
        }

        Cell& operator=(const Cell& other) = default;

        Size operator[](const Size vi) const {
            SPH_ASSERT(vi < 4);
            return idxs[vi];
        }

        Size& operator[](const Size vi) {
            SPH_ASSERT(vi < 4);
            return idxs[vi];
        }

        Handle neighbor(const Size fi) const {
            return neighs[fi];
        }

        void setNeighbor(const Size fi, const Handle& ch) {
#ifdef SPH_DEBUG
            for (Size fi2 = 0; fi2 < 4; ++fi2) {
                if (fi2 == fi) {
                    continue;
                }
                Handle nch = neighs[fi2];
                SPH_ASSERT(!nch || nch != ch);
            }
#endif
            neighs[fi] = ch;
        }

        bool contains(const Size vi) const {
            for (Size i : idxs) {
                if (vi == i) {
                    return true;
                }
            }
            return false;
        }

        Size neighborCnt() const {
            Size cnt = 0;
            for (Size i = 0; i < 4; ++i) {
                if (neighs[i]) {
                    cnt++;
                }
            }
            return cnt;
        }

        void detach() {
            for (Size fi1 = 0; fi1 < 4; ++fi1) {
                Handle nh = neighs[fi1];
                if (!nh) {
                    continue;
                }
                /// \todo store mirror indices
                for (Size fi2 = 0; fi2 < 4; ++fi2) {
                    if (nh->neighs[fi2] == this) {
                        nh->setNeighbor(fi2, nullptr);
                        neighs[fi1] = nullptr;
                        break;
                    }
                    SPH_ASSERT(fi2 != 3); // should never reach the end of the loop
                }
            }
        }

        bool isDetached() const {
            return neighborCnt() == 0;
        }

        const Sphere& circumsphere() const {
            return sphere;
        }
    };

    Array<Cell::Handle> cells;
    Size cellCnt = 0;

    // using Polyhedron = Array<std::pair<Cell::Handle, Size>>;


    FlatSet<Cell::Handle> badSet;
    // Polyhedron polyhedron;
    FlatSet<Cell::Handle> invalidated;

public:
    Delaunay() = default;
    ~Delaunay() {
        for (Cell::Handle ch : cells) {
            ch->detach();
            alignedDelete(ch);
        }
    }

    void build(ArrayView<const Vector> points) {
        vertices.clear();
        cells.clear();
        Array<Vector> sortedPoints;
        sortedPoints.insert(0, points.begin(), points.end());
        spatialSort(sortedPoints);

        Box box;
        for (const Vector& p : sortedPoints) {
            box.extend(p);
        }
        const Vector center = box.center();
        const Float side = 4._f * maxElement(box.size());
        Tetrahedron super = Tetrahedron::unit();
        for (Size i = 0; i < 4; ++i) {
            const Vector p = super.vertex(i) * side + center;
            vertices.push(p);
        }
        Cell::Handle root = alignedNew<Cell>(0, 1, 2, 3, super.circumsphere().value());
        cellCnt = 1;
        // cells.insert(ch);
        SPH_ASSERT(tetrahedron(*root).contains(super.center()));
        SPH_ASSERT(tetrahedron(*root).signedVolume() > 0._f);

#ifdef SPH_DEBUG
        const Float volume0 = tetrahedron(*root).volume();
        std::cout << "Volume = " << volume0 << std::endl;
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
                alignedDelete(cells[ci]);
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
    Cell::Handle addPoint(const Vector& p, const Cell::Handle& hint) {
        // implements the Bowyer–Watson algorithm

        vertices.push(p);

        const Cell::Handle seed = this->locate(
            p, hint, [](const Cell& c, const Vector& p) { return c.circumsphere().contains(p); });

        badSet.clear();
        this->region(
            seed, inserter(badSet), [this, &p](const Cell& c) { return c.circumsphere().contains(p); });


        // polyhedron.clear();
        invalidated.clear();
        Cell::Handle nextHint = nullptr;
        for (const Cell::Handle& ch : badSet) {
            SPH_ASSERT(ch->circumsphere().contains(p));
            for (Size fi = 0; fi < 4; ++fi) {
                const Cell::Handle nh = ch->neighbor(fi);
                if (!nh || !badSet.contains(nh)) {
                    // polyhedron.emplaceBack(ch, fi);
                    nextHint = this->triangulate(ch, fi, p);
                    if (nh) {
                        invalidated.insert(nh);
                    }
                }
            }
        }

        cellCnt -= badSet.size();
        for (Cell::Handle ch : badSet) {
            ch->detach();
        }

        // Cell::Handle nextHint = triangulatePolyhedron(p);

        updateConnectivity();

        for (Cell::Handle ch : badSet) {
            alignedDelete(ch);
        }

        return nextHint;
    }

    Cell::Handle triangulate(const Cell::Handle ch1, const Size fi1, const Vector& p) {
        // Cell::Handle nextHint = nullptr;
        // for (const auto& pair : polyhedron) {
        // const Cell::Handle& ch1 = pair.first;
        // const Size fi1 = pair.second;
        Face f1 = face(*ch1, fi1);
        const Triangle tri = triangle(f1);
        const Plane plane(tri);
        Cell::Handle ch2;
        Tetrahedron tet(triangle(f1), vertices.back());
        Optional<Sphere> sphere = tet.circumsphere();
        if (!sphere) {
            // small perturbation
            const Float e10 = getSqrLength(tri[1] - tri[0]);
            const Float e20 = getSqrLength(tri[2] - tri[0]);
            const Float e = sqrt(max(e10, e20));
            vertices.back() += 1.e-6_f * plane.normal() * e;
            sphere = tet.circumsphere();
            SPH_ASSERT(sphere);
        }
        if (!plane.above(p)) {
            std::swap(f1[1], f1[2]);
        }
        ch2 = alignedNew<Cell>(f1[0], f1[1], f1[2], vertices.size() - 1, sphere.value());

        SPH_ASSERT(tetrahedron(*ch2).signedVolume() > 0);
        SPH_ASSERT(tetrahedron(*ch2).contains(tet.center()));

        invalidated.insert(ch2);
        cellCnt++;
        //  nextHint = ch2;
        //  }

        // cellCnt += polyhedron.size();

        return ch2;
    }

    void updateConnectivity() {
        for (Size ci1 = 0; ci1 < invalidated.size(); ++ci1) {
            const Cell::Handle& ch1 = invalidated[ci1];
            // if it is in the badSet, it is already deleted
            SPH_ASSERT(!badSet.contains(ch1));
            for (Size ci2 = ci1 + 1; ci2 < invalidated.size(); ++ci2) {
                const Cell::Handle& ch2 = invalidated[ci2];
                SPH_ASSERT(!badSet.contains(ch2));

                for (Size fi1 = 0; fi1 < 4; ++fi1) {
                    for (Size fi2 = 0; fi2 < 4; ++fi2) {
                        if (opposite(face(*ch1, fi1), face(*ch2, fi2))) {
                            setNeighbor(ch1, fi1, ch2, fi2);
                        }
                    }
                }
            }
        }
    }

    void setNeighbor(const Cell::Handle& ch1, const Size fi1, const Cell::Handle& ch2, const Size fi2) {
        SPH_ASSERT(!ch1 || !ch2 || opposite(face(*ch1, fi1), face(*ch2, fi2)));
        if (ch1) {
            ch1->setNeighbor(fi1, ch2);
        }
        if (ch2) {
            ch2->setNeighbor(fi2, ch1);
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
    void region(const Cell::Handle& seed, TOutIter out, const TPredicate& predicate) const {
        Array<Cell::Handle> stack;
        std::unordered_set<Cell::Handle> visited;
        stack.push(seed);
        visited.insert(seed);

        while (!stack.empty()) {
            Cell::Handle ch = stack.pop();
            *out = ch;
            ++out;
            // SPH_ASSERT(!visited.contains(ch));
            // visited.insert(ch);

            for (Size fi = 0; fi < 4; ++fi) {
                Cell::Handle nh = ch->neighbor(fi);
                if (!nh || (visited.find(nh) != visited.end())) {
                    continue;
                }
                if (predicate(*nh)) {
                    visited.insert(nh);
                    stack.push(nh);
                }
            }
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
