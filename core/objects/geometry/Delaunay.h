#pragma once

/// \file Delaunay.h
/// \brief Delaunay triangulation (tetrahedronization) in three dimensions
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/containers/AdvancedAllocators.h"
#include "objects/geometry/Sphere.h"
#include "objects/geometry/Triangle.h"
#include "objects/utility/Progressible.h"
#include "objects/wrappers/Flags.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

class ILogger;

/// \brief Represents a tetrahedron, given by four points in three-dimensional space.
class Tetrahedron {
private:
    StaticArray<Vector, 4> vertices;

public:
    Tetrahedron() = default;

    /// \brief Creates the tetrahedron from its four vertices.
    Tetrahedron(const Vector& v1, const Vector& v2, const Vector& v3, const Vector& v4);

    /// \brief Creates the tetrahedron from an array of four vertices.
    Tetrahedron(const StaticArray<Vector, 4>& vertices);

    /// \brief Creates the tetrahedron given a triangle and an opposite vertex.
    Tetrahedron(const Triangle& tri, const Vector& v);

    Vector& vertex(const Size i) {
        SPH_ASSERT(i < 4, i);
        return vertices[i];
    }

    const Vector& vertex(const Size i) const {
        SPH_ASSERT(i < 4, i);
        return vertices[i];
    }

    /// \brief Returns the triangle for given face index.
    ///
    /// The triangle for given index lies opposite to the vertex with the same index.
    Triangle triangle(const Size fi) const;

    /// \brief Computes the signed volume of the tetrahedron.
    Float signedVolume() const;

    /// \brief Computes the absolute volume of the tetrahedron.
    Float volume() const;

    /// \brief Returns the centroid (center of mass) of the tetrahedron.
    Vector center() const;

    /// \brief Computes the circumsphere of the tetrahedron.
    ///
    /// The function returns NOTHING if the tetrahedron is degenerated.
    Optional<Sphere> circumsphere() const;

    /// \brief Checks if given point lies inside the tetrahedron.
    ///
    /// The tetrahedron must be oriented 'inside', i.e. it must have positive signed volume. This is checked
    /// by assert.
    bool contains(const Vector& p) const;

    /// \brief Returns a regular tetrahedron inscribed to unit sphere.
    ///
    /// The side length of the returned tetrahedron is sqrt(8/3).
    static Tetrahedron unit();

private:
    Optional<Vector> circumcenter() const;
};

class Delaunay : public Progressible<> {
public:
    /// \brief Represents a triangular face in the triangulation.
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

        /// \brief Returns the index of given vertex in the triangulation.
        Size operator[](const Size vi) const {
            SPH_ASSERT(vi < 3);
            return idxs[vi];
        }

        /// \brief Returns the index of given vertex in the triangulation.
        Size& operator[](const Size vi) {
            SPH_ASSERT(vi < 3);
            return idxs[vi];
        }

        /// \brief Returns the opposite face (i.e. same face belonging to the neighboring cell).
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

    /// \brief Represents a tetrahedronal cell
    class Cell {
        friend class Delaunay;

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

        Cell(const Size a, const Size b, const Size c, const Size d, const Sphere& sphere);

        Cell(const Cell& other) = delete;

        ~Cell() {
            SPH_ASSERT(this->isDetached());
        }

        Cell& operator=(const Cell& other) = delete;

        /// \brief Returns the index of given vertex in the triangulation.
        Size operator[](const Size vi) const {
            SPH_ASSERT(vi < 4);
            return idxs[vi];
        }

        /// \brief Returns the index of given vertex in the triangulation.
        Size& operator[](const Size vi) {
            SPH_ASSERT(vi < 4);
            return idxs[vi];
        }

        /// \brief Returns the face for given face index.
        ///
        /// The face with given index is opposite to the vertex with the same index.
        Face face(const Size fi) const;

        /// \brief Returns the neighboring cell for given face index, or nullptr if there is no neighbor.
        Handle neighbor(const Size fi) const;

        /// \brief Returns the number of existing neighbors.
        Size getNeighborCnt() const;

        /// \brief Returns the mirror index for given face.
        ///
        /// The mirror index is the index of this cell in the neighboring cell, i.e.
        /// <code>
        /// this == neighbor(fi)->neighbor(mirror(fi))
        /// </code>
        Size mirror(const Size fi) const;

    private:
        const Sphere& circumsphere() const {
            return sphere;
        }

        bool visited() const {
            return flag;
        }

        void setVisited(bool value) {
            flag = value;
        }

        void setNeighbor(const Size fi, const Handle& ch, const Size mirror);

        void detach();

        bool isDetached() const {
            return getNeighborCnt() == 0;
        }
    };

private:
    Array<Vector> vertices;
    Array<Cell::Handle> cells;
    Size cellCnt = 0;

    Array<Tuple<Cell::Handle, Size, Face>> added;

    Array<Cell::Handle> stack;
    Array<Cell::Handle> visited;
    Array<Cell::Handle> badSet;

    using Resource = MonotonicMemoryResource<Mallocator>;
    using Allocator = FallbackAllocator<MemoryResourceAllocator<Resource>, Mallocator>;

    Resource resource;
    Allocator allocator;

    AutoPtr<ILogger> logger;

public:
    /// \brief Creates an empty triangulation.
    ///
    /// \param allocatedMemory Size of the pre-allocated buffer, used to avoid frequent allocations.
    explicit Delaunay(const std::size_t allocatorMemory = 1 << 30);

    ~Delaunay();

    enum class BuildFlag {
        /// Reorders to input points to improve the spatial locality
        SPATIAL_SORT = 1 << 0,
    };

    /// \brief Builds the triangulation from given list of points.
    ///
    /// This replaces any previous triangulation.
    void build(ArrayView<const Vector> points, const Flags<BuildFlag> flags = BuildFlag::SPATIAL_SORT);

    /// \brief Returns the i-th cell.
    ///
    /// This call is only valid after the triangulation is created.
    Cell::Handle getCell(const Size i) const {
        return cells[i];
    }

    /// \brief Returns the total number of cells in the triangulation.
    Size getCellCnt() const {
        return cells.size();
    }

    /// \brief Returns the tetrahedron for given cell.
    Tetrahedron tetrahedron(const Cell& c) const {
        return Tetrahedron(vertices[c[0]], vertices[c[1]], vertices[c[2]], vertices[c[3]]);
    }

    /// \brief Returns the triangle for given face.
    Triangle triangle(const Face& f) const {
        return Triangle(vertices[f[0]], vertices[f[1]], vertices[f[2]]);
    }

    /// \brief Returns the convex hull of added points.
    Array<Triangle> convexHull() const;

    /// \brief Returns the alpha-shape of added points, given the value alpha.
    Array<Triangle> alphaShape(const Float alpha) const;

    /// \brief Finds the cell containing given point.
    ///
    /// The point must lie inside the convex hull, checked by assert.
    /// \param p Point to locate
    /// \param hint Optional hint where the search should start
    Cell::Handle locate(const Vector& p, const Cell::Handle hint = nullptr) const;

private:
    void buildImpl(ArrayView<const Vector> points);

    Cell::Handle addPoint(const Vector& p, const Cell::Handle hint);

    Cell::Handle triangulate(const Cell::Handle ch1, const Size fi1, const Vector& p);

    void updateConnectivity() const;

    void setNeighbors(const Cell::Handle ch1, const Size fi1, const Cell::Handle ch2, const Size fi2) const;

    template <typename TInsideFunc>
    Cell::Handle locate(const Vector& p, const Cell::Handle seed, const TInsideFunc& inside) const;

    template <typename TOutIter, typename TPredicate>
    void region(const Cell::Handle seed, TOutIter out, const TPredicate& predicate);

    template <typename TInsideFunc>
    Array<Triangle> surface(const TInsideFunc& func) const;
};

NAMESPACE_SPH_END
