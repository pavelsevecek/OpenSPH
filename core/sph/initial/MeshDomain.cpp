#include "sph/initial/MeshDomain.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

MeshDomain::MeshDomain(IScheduler& scheduler, Array<Triangle>&& triangles, const MeshParams& params) {
    Array<BvhTriangle> bvhTriangles;
    for (Triangle& t : triangles) {
        // transform vertices in place
        for (Size i = 0; i < 3; ++i) {
            t[i] = params.matrix * t[i];
        }

        bvhTriangles.emplaceBack(t[0], t[1], t[2]);
        cached.box.extend(t.getBBox());
    }
    const Vector center = cached.box.center();

    // compute volume (using center for optimal accuracy)
    cached.volume = 0._f;
    cached.area = 0._f;
    for (const Triangle& t : triangles) {
        cached.volume += dot(t[0] - center, cross(t[1] - center, t[2] - center)) / 6._f;
        cached.area += t.area();
        cached.points.push(t.center());
        cached.normals.push(t.normal());
    }
    bvh.build(std::move(bvhTriangles));

    if (params.precomputeInside) {
        Size res = params.volumeResolution;
        mask = Volume<char>(cached.box, res);

        parallelFor(scheduler, 0, res, [this, res](const Size z) {
            for (Size y = 0; y < res; ++y) {
                for (Size x = 0; x < res; ++x) {
                    mask.cell(x, y, z) =
                        containImpl(cached.box.lower() + cached.box.size() / res * Vector(x, y, z)) ? 1 : 0;
                }
            }
        });
    }
}

bool MeshDomain::contains(const Vector& v) const {
    if (mask.empty()) {
        return this->containImpl(v);
    } else {
        return mask(v) > 0;
    }
}

void MeshDomain::getSubset(ArrayView<const Vector> vs, Array<Size>& output, const SubsetType type) const {
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
        break;
    default:
        NOT_IMPLEMENTED;
    }
}

void MeshDomain::project(ArrayView<Vector>, Optional<ArrayView<Size>>) const {
    NOT_IMPLEMENTED;
}

void MeshDomain::addGhosts(ArrayView<const Vector> vs,
    Array<Ghost>& ghosts,
    const Float UNUSED(eta),
    const Float UNUSED(eps)) const {
    ghosts.clear();
    const Float h = vs[0][H];
    for (Size i = 0; i < cached.points.size(); ++i) {
        ghosts.push(Ghost{ cached.points[i] - h * cached.normals[i], 0 });
    }
}

bool MeshDomain::containImpl(const Vector& v) const {
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
    SPH_ASSERT(insideCnt + outsideCnt == 6);
    return insideCnt >= outsideCnt;
}

NAMESPACE_SPH_END
