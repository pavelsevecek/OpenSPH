#pragma once

#include "math/AffineMatrix.h"
#include "objects/Exceptions.h"
#include "objects/containers/FlatMap.h"
#include "objects/containers/FlatSet.h"
#include "objects/geometry/Plane.h"
#include "objects/geometry/Sphere.h"
#include "objects/utility/IteratorAdapters.h"
#include "objects/wrappers/SharedPtr.h"
#include <iostream>

NAMESPACE_SPH_BEGIN

struct SingularMatrix : public Exception {
    SingularMatrix()
        : Exception("Cannot compute circumcenter") {}
};

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

    Sphere circumsphere() const {
        const Vector center = this->circumcenter();
        const Float radius = getLength(vertices[0] - center);
        SPH_ASSERT(radius < LARGE);

#ifdef SPH_DEBUG
        for (Size i = 1; i < 4; ++i) {
            const Float altRadius = getLength(vertices[i] - center);
            SPH_ASSERT(almostEqual(radius, altRadius, 1.e-8_f), radius, altRadius);
        }
#endif
        const Sphere sphere(center, radius);
        return sphere;
    }

    bool inside(const Vector& p) {
        for (Size fi = 0; fi < 4; ++fi) {
            if (!Plane(triangle(fi)).above(p)) {
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
    Vector circumcenter() const {
        const Vector d1 = vertices[1] - vertices[0];
        const Vector d2 = vertices[2] - vertices[0];
        const Vector d3 = vertices[3] - vertices[0];
        AffineMatrix A(d1, d2, d3);
        SPH_ASSERT(A.translation() == Vector(0._f));
        Optional<AffineMatrix> A_inv = A.tryInverse();
        if (!A_inv) {
            throw SingularMatrix();
        }

        const Vector B = 0.5_f * Vector(getSqrLength(d1), getSqrLength(d2), getSqrLength(d3));
        return A_inv.value() * B + vertices[0];
    }
};

class Delaunay {
private:
    Array<Vector> vertices;

    class Cell {
    public:
        using Handle = SharedPtr<Cell>;

    private:
        StaticArray<Size, 4> idxs;
        Sphere sphere;

        StaticArray<WeakPtr<Cell>, 4> neighs;

    public:
        Cell() = default;

        Cell(const Size a, const Size b, const Size c, const Size d, const Sphere& sphere)
            : idxs({ a, b, c, d })
            , sphere(sphere) {
#ifdef SPH_DEBUG
            StaticArray<Size, 4> sortedIdxs({ a, b, c, d });
            std::sort(sortedIdxs.begin(), sortedIdxs.end());
            SPH_ASSERT(std::unique(sortedIdxs.begin(), sortedIdxs.end()) == sortedIdxs.end());
#endif
        }

        Cell(const Cell& other)
            : idxs(other.idxs.clone())
            , sphere(other.sphere)
            , neighs(other.neighs.clone()) {}

        Cell& operator=(const Cell& other) {
            idxs = other.idxs.clone();
            neighs = other.neighs.clone();
            sphere = other.sphere;
            return *this;
        }

        Size operator[](const Size vi) const {
            SPH_ASSERT(vi < 4);
            return idxs[vi];
        }

        Size& operator[](const Size vi) {
            SPH_ASSERT(vi < 4);
            return idxs[vi];
        }

        const Handle neighbor(const Size fi) const {
            return neighs[fi].lock();
        }

        void setNeighbor(const Size fi, const Handle& ch) {
#ifdef SPH_DEBUG
            for (Size fi2 = 0; fi2 < 4; ++fi2) {
                if (fi2 == fi) {
                    continue;
                }
                Handle nch = neighs[fi2].lock();
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
                if (neighs[i].lock()) {
                    cnt++;
                }
            }
            return cnt;
        }

        const Sphere& circumsphere() const {
            return sphere;
        }

        bool operator<(const Cell& other) const {
            return std::lexicographical_compare(
                idxs.begin(), idxs.end(), other.idxs.begin(), other.idxs.end());
        }
    };

    Array<Cell::Handle> cells;

    using Face = StaticArray<Size, 3>;

public:
    void build(ArrayView<const Vector> points) {
        vertices.clear();
        cells.clear();

        Box box;
        for (const Vector& p : points) {
            box.extend(p);
        }
        const Vector center = box.center();
        const Float side = 4._f * maxElement(box.size());
        Tetrahedron super = Tetrahedron::unit();
        for (Size i = 0; i < 4; ++i) {
            const Vector p = super.vertex(i) * side + center;
            vertices.push(p);
        }
        cells.push(makeShared<Cell>(0, 1, 2, 3, super.circumsphere()));

#ifdef SPH_DEBUG
        const Float volume0 = tetrahedron(0).volume();
        std::cout << "Volume = " << volume0 << std::endl;
#endif

        Size index = 0;
        for (const Vector& p : points) {
            std::cout << "Adding point " << index++ << " out of " << points.size() << ", cell count "
                      << cells.size() << std::endl;

            Cell::Handle hint = nullptr;
            try {
                hint = this->addPoint(p, hint);
            } catch (const SingularMatrix&) {
                std::cout << "Cannot compute circumcenter, skipping the point ..." << std::endl;
            }

#ifdef SPH_DEBUG
            Float volume = 0._f;
            for (Size ci = 0; ci < cells.size(); ++ci) {
                volume += tetrahedron(ci).volume();
            }
            SPH_ASSERT(almostEqual(volume, volume0), volume, volume0);
#endif
        }

        Array<Size> toRemove;
        for (Size ci = 0; ci < cells.size(); ++ci) {
            Cell& c = *cells[ci];
            if (c[0] < 4 || c[1] < 4 || c[2] < 4 || c[3] < 4) {
                toRemove.push(ci);
            }
        }
        // std::cout << "Removing " << toRemove.size() << " super-tets" << std::endl;
        cells.remove(toRemove);
    }

    Tetrahedron tetrahedron(const Size ci) const {
        const Cell& c = *cells[ci];
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

        FlatSet<Cell::Handle> badSet;
        Array<Size> badIdxs;
        /*StaticArray<Cell::Handle, 5> hints;
        hints[0] = hint;
        for (Size fi = 0; fi < 4; ++fi) {
            hints[fi + 1] = hint->neighbor(fi);
        }
        for (const Cell::Handle& ch : hints) {
            if (!ch) {
                continue;
            }
            if (ch->circumsphere().contains(p)) {
                // good hint
                region(ch, [](const Cell& c) {

                })
            }
        }*/
        (void)hint;
        if (badSet.empty()) {
            for (Size ci = 0; ci < cells.size(); ++ci) {
                const Sphere sphere = cells[ci]->circumsphere();
                if (sphere.contains(p)) {
                    badSet.insert(cells[ci]);
                    badIdxs.push(ci);
                }
            }
        }
        // std::cout << "Bad set has " << badSet.size() << " cells" << std::endl;

        Array<std::pair<Cell::Handle, Size>> polyhedronSet;
        for (Cell::Handle ch : badSet) {
            for (Size fi = 0; fi < 4; ++fi) {
                const Cell::Handle nh = ch->neighbor(fi);
                if (!nh || !badSet.contains(nh)) {
                    polyhedronSet.emplaceBack(ch, fi);
                }
            }
        }
        // std::cout << "Removing " << badIdxs.size() << " cells" << std::endl;
        for (Size ci : reverse(badIdxs)) {
            /*for (Size fi1 = 0; fi1 < 4; ++fi1) {
                // remove the cell from its neighbors
                if (Cell::Handle nh = cells[ci]->neighbor(fi1)) {
                    for (Size fi2 = 0; fi2 < 4; ++fi2) {
                        if (nh->neighbor(fi2) == cells[ci]) {
                            nh->setNeighbor(fi2, nullptr);
                            break;
                        }
                        SPH_ASSERT(fi2 < 3);
                    }
                }
                // remove the neighbors
                cells[ci]->setNeighbor(fi1, nullptr);
            }*/

            std::swap(cells[ci], cells.back());
            cells.pop();
        }
        // badSet.clear();

        vertices.push(p);
        FlatSet<Cell::Handle> added;
        Cell::Handle nextHint;
        for (const auto& pair : polyhedronSet) {
            const Cell::Handle& ch1 = pair.first;
            const Size fi1 = pair.second;
            const Face f1 = face(*ch1, fi1);
            const Plane plane(triangle(f1));
            Cell::Handle ch2;
            Tetrahedron tet(triangle(f1), vertices.back());
            if (!plane.above(p)) {
                ch2 = makeShared<Cell>(f1[0], f1[1], f1[2], vertices.size() - 1, tet.circumsphere());
            } else {
                ch2 = makeShared<Cell>(f1[0], f1[2], f1[1], vertices.size() - 1, tet.circumsphere());
            }

            cells.push(ch2);
            // Float sgnVolume = tetrahedron(cells.size() - 1).signedVolume();
            // SPH_ASSERT(sgnVolume >= 0._f, sgnVolume);
            added.insert(ch2);
            // for (Size fi = 0; fi < 4; ++fi) {
            if (Cell::Handle nch = ch1->neighbor(fi1)) {
                // SPH_ASSERT(!badSet.contains(nch));
                if (!badSet.contains(nch)) {
                    added.insert(nch);
                }
            }
            if (!nextHint) {
                nextHint = ch2;
            }
            //}
        }

        /// \todo create in advance a set of cells that need to be updated, remove the neighbors from badSet
        /// cells

        // fix connectivity of the new cells
        for (Size ci1 = 0; ci1 < added.size(); ++ci1) {
            // SPH_ASSERT(!badSet.contains(ch1));
            const Cell::Handle& ch1 = added[ci1];
            if (badSet.contains(ch1)) {
                continue;
            }
            for (Size ci2 = ci1 + 1; ci2 < added.size(); ++ci2) {
                const Cell::Handle& ch2 = added[ci2];
                if (badSet.contains(ch2)) {
                    continue;
                }
                // SPH_ASSERT(!badSet.contains(ch2));

                for (Size fi1 = 0; fi1 < 4; ++fi1) {
                    for (Size fi2 = 0; fi2 < 4; ++fi2) {
                        if (opposite(face(*ch1, fi1), face(*ch2, fi2))) {
                            setNeighbor(ch1, fi1, ch2, fi2);
                        }
                    }
                }
            }
        }

        return nextHint;
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
            return { c[1], c[2], c[3] };
        case 1:
            return { c[0], c[3], c[2] };
        case 2:
            return { c[0], c[1], c[3] };
        case 3:
            return { c[0], c[2], c[1] };
        default:
            STOP;
        }
    }

    bool opposite(const Face& f1, const Face& f2) const {
        for (Size i1 = 0; i1 < 3; ++i1) {
            for (Size j1 = 0; j1 < 3; ++j1) {
                if (f1[i1] == f2[j1]) {
                    const Size i2 = (i1 + 1) % 3;
                    const Size i3 = (i1 + 2) % 3;
                    const Size j2 = (j1 + 1) % 3;
                    const Size j3 = (j1 + 2) % 3;
                    if (f1[i2] == f2[j3] && f1[i3] == f2[j2]) {
                        return true;
                    }
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
        /*Array<Cell::Handle> inside;
        for (const Cell::Handle& ch : cells) {
            if (func(*ch)) {
                inside.push(ch);
            }
        }
        Array<Cell::Handle> */

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
                triangles.push(tr.inverted());
            }
        }
        return triangles;
    }

    template <typename TPredicate>
    void region(const Cell::Handle& seed, Array<Cell::Handle>& matching, const TPredicate& predicate) {
        Array<Cell::Handle> stack;
        FlatSet<Cell::Handle> visited;
        stack.push(seed);

        while (!stack.empty()) {
            Cell::Handle ch = stack.pop();
            matching.push(ch);
            SPH_ASSERT(!visited.contains(ch));
            visited.insert(ch);

            for (Size fi = 0; fi < 4; ++fi) {
                Cell::Handle nh = ch->neighbor(fi);
                if (!nh || visited.contains(nh)) {
                    continue;
                }
                if (predicate(*nh)) {
                    stack.push(nh);
                }
            }
        }
    }
};

NAMESPACE_SPH_END
