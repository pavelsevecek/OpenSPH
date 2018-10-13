#pragma once

#include "objects/finders/Bvh.h"
#include "objects/geometry/Domain.h"
#include "objects/geometry/Triangle.h"

NAMESPACE_SPH_BEGIN

/// \todo doesn't really belong to Post


class MeshDomain : public IDomain {
private:
    Bvh<BvhTriangle> bvh;

    /// Cached values, so that we do not have to keep a separate list of triangles
    struct {
        Box box;
        Float volume;
    } cached;

public:
    explicit MeshDomain(Array<Triangle>&& triangles, const AffineMatrix matrix = AffineMatrix::identity())
        : IDomain(Vector(0._f)) {
        Array<BvhTriangle> bvhTriangles;
        for (Triangle& t : triangles) {
            // transform vertices in place
            for (Size i = 0; i < 3; ++i) {
                t[i] = matrix * t[i];
            }

            bvhTriangles.emplaceBack(t[0], t[1], t[2]);
            cached.box.extend(t.getBBox());
        }
        center = cached.box.center();

        // compute volume (using center for optimal accuracy)
        cached.volume = 0._f;
        for (const Triangle& t : triangles) {
            cached.volume += dot(t[0] - center, cross(t[1] - center, t[2] - center)) / 6._f;
        }
        bvh.build(std::move(bvhTriangles));
    }

    virtual Box getBoundingBox() const override {
        return cached.box;
    }

    virtual Float getVolume() const override {
        return cached.volume;
    }

    virtual bool contains(const Vector& v) const override {
        // As we assume watertight mesh, we could theoretically make just one intersection test, but this
        // could cause problems at grazing angles, returning false positives. Instead, we opt for a more
        // robust (albeit slower) solution and cast a ray for each axis.
        Size insideCnt = 0;
        Size outsideCnt = 0;
        static Array<Vector> dirs = {
            Vector(1._f, 0._f, 0._f),
            Vector(-1._f, 0._f, 0._f),
            Vector(0._f, 1._f, 0._f),
            Vector(0._f, -1._f, 0._f),
            Vector(0._f, 0._f, 1._f),
            Vector(0._f, 0._f, -1._f),
        };

        std::set<IntersectionInfo> intersections;
        for (Vector& dir : dirs) {
            Ray ray(v, dir);
            bvh.getAllIntersections(ray, intersections);
            if (isOdd(intersections.size())) {
                insideCnt++;
            } else {
                outsideCnt++;
            }
        }
        ASSERT(insideCnt + outsideCnt == 6);
        return insideCnt >= outsideCnt;
    }

    virtual void getSubset(ArrayView<const Vector>, Array<Size>&, const SubsetType) const override {
        NOT_IMPLEMENTED;
    }

    virtual void getDistanceToBoundary(ArrayView<const Vector>, Array<Float>&) const override {
        NOT_IMPLEMENTED;
    }

    virtual void project(ArrayView<Vector>, Optional<ArrayView<Size>>) const override {
        NOT_IMPLEMENTED;
    }

    virtual void addGhosts(ArrayView<const Vector>, Array<Ghost>&, const Float, const Float) const override {
        NOT_IMPLEMENTED;
    }
};

NAMESPACE_SPH_END
