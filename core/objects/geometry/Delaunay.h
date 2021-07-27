#pragma once

#include "math/AffineMatrix.h"
#include "objects/Exceptions.h"
#include "objects/containers/FlatMap.h"
#include "objects/containers/FlatSet.h"
#include "objects/geometry/Plane.h"
#include "objects/geometry/Sphere.h"
#include "objects/utility/IteratorAdapters.h"
#include "objects/wrappers/SharedPtr.h"
#include "post/MeshFile.h"
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

    Tetrahedron(const StaticArray<Vector, 4>& vertices)
        : vertices(vertices.clone()) {}

    Vector& vertex(const Size i) {
        return vertices[i];
    }

    const Vector& vertex(const Size i) const {
        return vertices[i];
    }

    Triangle triangle(const Size i) const {
        switch (i) {
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
            SPH_ASSERT(almostEqual(radius, altRadius), radius, altRadius);
        }
#endif
        const Sphere sphere(center, radius);
        return sphere;
    }

    static Tetrahedron unit() {
        return Tetrahedron({
            Vector(sqrt(8._f / 9._f), 0, -1._f / 3._f),
            Vector(-sqrt(2._f / 9._f), sqrt(2._f / 3._f), -1._f / 3._f),
            Vector(-sqrt(2._f / 9._f), -sqrt(2._f / 3._f), -1._f / 3._f),
            Vector(0, 0, 1),
        });
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
    Float refVolume;

    class Cell {
    public:
        using Handle = SharedPtr<Cell>;

    private:
        StaticArray<Size, 4> idxs;
        StaticArray<WeakPtr<Cell>, 4> neighs;

    public:
        Cell() = default;

        Cell(const Size a, const Size b, const Size c, const Size d)
            : idxs({ a, b, c, d }) {
#ifdef SPH_DEBUG
            StaticArray<Size, 4> sortedIdxs({ a, b, c, d });
            std::sort(sortedIdxs.begin(), sortedIdxs.end());
            SPH_ASSERT(std::unique(sortedIdxs.begin(), sortedIdxs.end()) == sortedIdxs.end());
#endif
        }

        Cell(const Cell& other)
            : idxs(other.idxs.clone())
            , neighs(other.neighs.clone()) {}

        Cell& operator=(const Cell& other) {
            idxs = other.idxs.clone();
            neighs = other.neighs.clone();
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

        bool isSuper() const {
            for (Size i = 0; i < 4; ++i) {
                if (!neighs[i].lock()) {
                    return true;
                }
            }
            return false;
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
        cells.push(makeShared<Cell>(0, 1, 2, 3));
        refVolume = tetrahedron(0).volume();
        std::cout << "Volume = " << refVolume << std::endl;

        Size index = 0;
        for (const Vector& p : points) {
            std::cout << "Adding point " << index++ << std::endl;
            /*Array<Triangle> triangles;
            for (Size i = 0; i < getTetrahedraCnt(); ++i) {
                Tetrahedron tet = tetrahedron(i);
                for (Size j = 0; j < 4; ++j) {
                    triangles.push(tet.triangle(j));
                }
            }
            PlyFile ply;
            ply.save(Path("bunny_" + std::to_string(index++) + ".ply"), triangles);*/

            try {
                this->addPoint(p);
            } catch (const SingularMatrix&) {
                std::cout << "Cannot compute circumcenter, skipping the point ..." << std::endl;
            }
        }

        Array<Size> toRemove;
        for (Size ci = 0; ci < cells.size(); ++ci) {
            Cell& c = *cells[ci];
            if (c[0] < 4 || c[1] < 4 || c[2] < 4 || c[3] < 4) {
                toRemove.push(ci);
            }
        }
        std::cout << "Removing " << toRemove.size() << " super-tets" << std::endl;
        cells.remove(toRemove);
    }

    Tetrahedron tetrahedron(const Size ci) const {
        const Cell& c = *cells[ci];
        return Tetrahedron({ vertices[c[0]], vertices[c[1]], vertices[c[2]], vertices[c[3]] });
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

private:
    void addPoint(const Vector& p) {
        // implements the Bowyer–Watson algorithm

        FlatSet<Cell::Handle> badSet;
        Array<Size> badIdxs;
        for (Size ci = 0; ci < cells.size(); ++ci) {
            const Sphere sphere = tetrahedron(ci).circumsphere();
            //  std::cout << "Circumsphere with center " << sphere.center() << ", radius = " <<
            //  sphere.radius()
            //            << std::endl;
            if (sphere.contains(p)) {
                badSet.insert(cells[ci]);
                badIdxs.push(ci);
            }
        }
        std::cout << "Bad set has " << badSet.size() << " cells" << std::endl;

        Array<std::pair<Cell::Handle, Size>> polyhedronSet;
        for (Cell::Handle ch : badSet) {
            for (Size fi = 0; fi < 4; ++fi) {
                const Cell::Handle nh = ch->neighbor(fi);
                if (!nh || !badSet.contains(nh)) {
                    // const Face f = face(*ch, fi);
                    // std::cout << "Connecting new vertex with face " << f[0] << "," << f[1] << "," << f[2]
                    //        << std::endl;
                    polyhedronSet.emplaceBack(ch, fi);
                }
            }
        }
        std::cout << "Removing " << badIdxs.size() << " cells" << std::endl;
        /*for (Size i : badIdxs) {
            std::cout << " bad cell " << i << std::endl;
        }*/
        cells.remove(badIdxs);

        vertices.push(p);
        FlatSet<Cell::Handle> added;
        for (const auto& pair : polyhedronSet) {
            const Cell::Handle& ch1 = pair.first;
            const Size fi1 = pair.second;
            const Face f1 = face(*ch1, fi1);
            const Plane plane(triangle(f1));
            Cell::Handle ch2;
            if (!plane.above(p)) {
                ch2 = makeShared<Cell>(f1[0], f1[1], f1[2], vertices.size() - 1);
            } else {
                ch2 = makeShared<Cell>(f1[0], f1[2], f1[1], vertices.size() - 1);
            }
            cells.push(ch2);
            // Float sgnVolume = tetrahedron(cells.size() - 1).signedVolume();
            // SPH_ASSERT(sgnVolume >= 0._f, sgnVolume);
            added.insert(ch2);
            /*if (auto nch = ch1->neighbor(fi1)) {
                added.insert(nch);
            }*/
            // neighbor at the face opposite to the new vertex
            /* if (auto nch = ch1->neighbor(fi1)) {
                 for (Size i = 0; i < 4; ++i) {
                     if (opposite(face(*ch2, 3), face(*nch, i))) {
                         setNeighbor(ch2, 3, nch, i);
                         break;
                     }
                 }
             }*/
        }


        // fix connectivity of the new cells
        for (Cell::Handle& ch1 : added) {
            for (Cell::Handle& ch2 : cells) {
                if (ch1 == ch2) {
                    continue;
                }

                for (Size fi1 = 0; fi1 < 4; ++fi1) {
                    for (Size fi2 = 0; fi2 < 4; ++fi2) {
                        if (opposite(face(*ch1, fi1), face(*ch2, fi2))) {
                            setNeighbor(ch1, fi1, ch2, fi2);
                        }
                    }
                }
            }
        }

        std::cout << "Added point " << p << ", cell count = " << cells.size() << std::endl;
        std::cout << "Cells: " << std::endl;
        Float volume = 0._f;
        for (Size ci = 0; ci < cells.size(); ++ci) {
            volume += tetrahedron(ci).volume();
        }
        SPH_ASSERT(almostEqual(volume, refVolume), volume, refVolume);

        /*   for (Cell::Handle ch : cells) {
               // SPH_ASSERT(ch->neighborCnt() >= 3);
               std::cout << (*ch)[0] << "," << (*ch)[1] << "," << (*ch)[2] << "," << (*ch)[3] << ", neighs "
                         << ch->neighborCnt() << " (";
               for (Size i = 0; i < 4; ++i) {
                   if (auto nch = ch->neighbor(i)) {
                       std::cout << (*nch)[0] << "," << (*nch)[1] << "," << (*nch)[2] << "," << (*nch)[3]
                                 << " and ";
                   }
               }
               std::cout << ")" << std::endl;
           }*/
    }


    void setNeighbor(const Cell::Handle& ch1, const Size fi1, const Cell::Handle& ch2, const Size fi2) {
        SPH_ASSERT(!ch1 || !ch2 || opposite(face(*ch1, fi1), face(*ch2, fi2)));
        /*if (ch1 && ch2) {
            std::cout << "Setting neighbors:\n";
            std::cout << (*ch1)[0] << "," << (*ch1)[1] << "," << (*ch1)[2] << "," << (*ch1)[3] << "\n";
            std::cout << (*ch2)[0] << "," << (*ch2)[1] << "," << (*ch2)[2] << "," << (*ch2)[3] << std::endl;
            std::cout << "Faces:\n";
            Face f1 = face(*ch1, fi1);
            Face f2 = face(*ch2, fi2);
            std::cout << f1[0] << "," << f1[1] << "," << f1[2] << std::endl;
            std::cout << f2[0] << "," << f2[1] << "," << f2[2] << std::endl;
        }*/
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

    /*  Vector circumcenter(const Cell& c) const {
          const Vector d1 = vertices[c[1]] - vertices[c[0]];
          const Vector d2 = vertices[c[2]] - vertices[c[0]];
          const Vector d3 = vertices[c[3]] - vertices[c[0]];
          AffineMatrix A(d1, d2, d3);
          SPH_ASSERT(A.translation() == Vector(0._f));

          const Vector B = 0.5_f * Vector(getSqrLength(d1), getSqrLength(d2), getSqrLength(d3));
          return A.inverse() * B;
      }

      Sphere circumsphere(const Cell& c) const {
          const Vector center = this->circumcenter(c);
          const Float radius = getLength(vertices[c[0]] - center);
  #ifdef SPH_DEBUG
          for (Size i = 1; i < 4; ++i) {
              const Float altRadius = getLength(vertices[c[i]] - center);
              SPH_ASSERT(radius == altRadius, radius, altRadius);
          }
  #endif
          return Sphere(center, radius);
      }*/

    Triangle triangle(const Face& f) const {
        return Triangle(vertices[f[0]], vertices[f[1]], vertices[f[2]]);
    }
};

NAMESPACE_SPH_END
