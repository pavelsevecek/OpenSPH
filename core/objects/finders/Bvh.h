#pragma once

#include "objects/containers/Array.h"
#include "objects/geometry/Box.h"
#include "objects/geometry/Indices.h"
#include "objects/wrappers/AutoPtr.h"

NAMESPACE_SPH_BEGIN

class Ray {
    friend bool intersectBox(const Box& box, const Ray& ray, Float& t_min, Float& t_max);

private:
    Vector orig;
    Vector dir;
    Vector invDir;
    Indices signs;

public:
    Ray() = default;

    Ray(const Vector& origin, const Vector& dir)
        : orig(origin)
        , dir(dir) {
        for (Size i = 0; i < 3; ++i) {
            invDir[i] = (dir[i] == 0._f) ? INFTY : 1._f / dir[i];
            signs[i] = int(invDir[i] < 0._f);
        }
    }

    const Vector& origin() const {
        return orig;
    }

    const Vector& direction() const {
        return dir;
    }
};

/// \brief Finds intersections of ray with a box.
///
/// There is always either zero or two intersections. If the ray intersects the box, the function returns true
/// and the intersection distances (near and far distance) are returned as t_min and t_max.
inline bool intersectBox(const Box& box, const Ray& ray, Float& t_min, Float& t_max);


struct BvhPrimitive {
    /// Generic user data, can be used to store additional information to the primitives.
    Size userData = Size(-1);
};

/// \brief Holds intormation about intersection.
struct IntersectionInfo {
    /// Distance of the hit in units of ray.direction().
    Float t;

    /// Object hit by the ray, or nullptr if nothing has been hit.
    const BvhPrimitive* object = nullptr;

    INLINE Vector hit(const Ray& ray) const {
        return ray.origin() + ray.direction() * t;
    }

    /// Sort by intersection distance
    INLINE bool operator<(const IntersectionInfo& other) const {
        return t < other.t;
    }
};

/// \brief Trait for finding intersections with a triangle
class BvhTriangle : public BvhPrimitive {
private:
    Vector v0;
    Vector dir1, dir2;

public:
    BvhTriangle(const Vector& v0, const Vector& v1, const Vector& v2)
        : v0(v0) {
        dir1 = v1 - v0;
        dir2 = v2 - v0;
    }

    INLINE bool getIntersection(const Ray& ray, IntersectionInfo& intersection) const {
        // https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm#C++_Implementation

        const Float eps = EPS * dot(dir1, dir2);
        const Vector h = cross(ray.direction(), dir2);
        const Float a = dot(dir1, h);
        if (a > -eps && a < eps) {
            return false;
        }
        const Float f = 1._f / a;
        const Vector s = ray.origin() - v0;
        const Float u = f * dot(s, h);
        if (u < 0._f || u > 1._f) {
            return false;
        }
        const Vector q = cross(s, dir1);
        const Float v = f * dot(ray.direction(), q);
        if (v < 0._f || u + v > 1._f) {
            return false;
        }
        intersection.object = this;
        intersection.t = f * dot(dir2, q);
        return intersection.t > 0.f;
    }

    INLINE Box getBBox() const {
        Box box;
        box.extend(v0);
        box.extend(v0 + dir1);
        box.extend(v0 + dir2);
        return box;
    }

    INLINE Vector getCenter() const {
        return v0 + (dir1 + dir2) / 3._f;
    }
};

/// \brief Trait for finding intersections with a sphere.
class BvhSphere : public BvhPrimitive {
private:
    Vector center;
    Float r;
    Float rSqr;

public:
    BvhSphere() = default;

    BvhSphere(const Vector& center, const Float radius)
        : center(center)
        , r(radius)
        , rSqr(radius * radius) {
        SPH_ASSERT(r > 0._f);
    }

    INLINE bool getIntersection(const Ray& ray, IntersectionInfo& intersection) const {
        const Vector delta = center - ray.origin();
        const Float deltaSqr = getSqrLength(delta);
        const Float deltaCos = dot(delta, ray.direction());
        const Float disc = sqr(deltaCos) - deltaSqr + rSqr;
        if (disc < 0._f) {
            return false;
        }

        intersection.object = this;
        intersection.t = deltaCos - sqrt(disc);
        return intersection.t > 0.f;
    }

    INLINE Box getBBox() const {
        return Box(center - Vector(r), center + Vector(r));
    }

    INLINE Vector getCenter() const {
        return center;
    }
};

/// \brief Trait for finding intersections with a axis-aligned box.
class BvhBox : public BvhPrimitive {
private:
    Box box;

public:
    explicit BvhBox(const Box& box)
        : box(box) {}

    INLINE bool getIntersection(const Ray& ray, IntersectionInfo& intersection) const {
        Float t_min, t_max;
        const bool result = intersectBox(box, ray, t_min, t_max);
        if (result) {
            intersection.t = t_min;
            intersection.object = this;
            return intersection.t > 0.f;
        } else {
            return false;
        }
    }

    INLINE Box getBBox() const {
        return box;
    }

    INLINE Vector getCenter() const {
        return box.center();
    }
};

/// \brief Simple bounding volume hierarchy.
///
/// Interface for finding an intersection of given ray with a set of geometric objects. Currently very
/// limited and not very optimized. \ref Bvh is explicitly specialized for \ref BvhSphere and \ref BvhBox; if
/// other geometric primitives are needed, either add the specialization to cpp, or move the implementation to
/// header.
template <typename TBvhObject>
class Bvh : public Noncopyable {
private:
    const Size leafSize;
    Size nodeCnt = 0;
    Size leafCnt = 0;

    Array<TBvhObject> objects;

    struct BvhNode {
        Box box;
        Size start;
        Size primCnt;
        Size rightOffset;
    };

    Array<BvhNode> nodes;

public:
    explicit Bvh(const Size leafSize = 4)
        : leafSize(leafSize) {}

    /// \brief Contructs the BVH from given set of objects.
    ///
    /// This erased previously stored objects.
    void build(Array<TBvhObject>&& objects);

    /// \brief Finds the closest intersection of the ray.
    ///
    /// Returns true if an intersection has been found.
    bool getFirstIntersection(const Ray& ray, IntersectionInfo& intersection) const;

    /// \brief Returns all intersections of the ray.
    ///
    /// Intersections are returned via a generic output iterator.
    /// \returns Total number of intersections.
    template <typename TOutIter>
    Size getAllIntersections(const Ray& ray, TOutIter iter) const;

    /// \brief Returns true if the ray is occluded by some geometry
    bool isOccluded(const Ray& ray) const;

    /// \brief Returns the bounding box of all objects in BVH.
    Box getBoundingBox() const;

private:
    template <typename TAddIntersection>
    void getIntersections(const Ray& ray, const TAddIntersection& addIntersection) const;
};

NAMESPACE_SPH_END

#include "objects/finders/Bvh.inl.h"
