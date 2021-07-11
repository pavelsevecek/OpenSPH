#pragma once

#include "objects/geometry/Box.h"
#include "objects/geometry/Sphere.h"
#include <unordered_map>

NAMESPACE_SPH_BEGIN

/// \brief Container of points with optimized search queries.
///
/// Works similarly to \ref HashMapFinder, but does not require rebuilding the structure when the set of
/// points changes.
class PointCloud : public Noncopyable {
private:
    using Cell = Array<Vector>;
    using HashMap = std::unordered_map<Indices, Cell, std::hash<Indices>, IndicesEqual>;

    HashMap map;
    Size count = 0;
    Float cellSize;

public:
    /// Identifies a point in the point cloud
    class Handle {
    private:
        Indices data;

    public:
        Handle() = default;

        Handle(const Indices& idxs, const Size index, Badge<PointCloud>)
            : data(idxs) {
            data[3] = index;
        }

        /// \brief Grid coordinates
        Indices coords() const {
            return data;
        }

        /// \brief Index of the point within the cell
        Size index() const {
            return data[3];
        }

        bool operator==(const Handle& other) const {
            return all(coords() == other.coords()) && index() == other.index();
        }

        bool operator!=(const Handle& other) const {
            return !(*this == other);
        }
    };

    explicit PointCloud(const Float cellSize)
        : cellSize(cellSize) {}

    /// \brief Adds a point into the cloud.
    ///
    /// \returns Handle that identifies the point within the cloud.
    Handle push(const Vector& p);

    /// \brief Adds a set of points into the cloud.
    void push(ArrayView<const Vector> points);

    /// \brief Returns the point corresponding to given handle.
    Vector point(const Handle& handle) const;

    /// \brief Returns all points in the cloud as array.
    Array<Vector> array() const;

    /// \brief Returns the number of points in the cloud.
    Size size() const;

    /// \brief Returns the number of points within given distance from the center point.
    Size getClosePointsCount(const Vector& center, const Float radius) const;

    /// \brief Returns point within given distance from the center point.
    void findClosePoints(const Vector& center, const Float radius, Array<Handle>& handles) const;

    /// \brief Returns point within given distance from the center point.
    void findClosePoints(const Vector& center, const Float radius, Array<Vector>& neighs) const;

private:
    template <typename TAdd>
    void findClosePoints(const Vector& center, const Float radius, const TAdd& add) const;

    INLINE Box cellBox(const Indices& idxs) const {
        return Box(Vector(idxs) * cellSize, Vector(idxs + Indices(1, 1, 1)) * cellSize);
    }
};

NAMESPACE_SPH_END
