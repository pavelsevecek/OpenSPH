#pragma once

#include "objects/finders/Bvh.h"
#include "objects/utility/IteratorAdapters.h"
#include "objects/utility/OutputIterators.h"
#include "physics/Constants.h"
#include "quantities/Attractor.h"

NAMESPACE_SPH_BEGIN

struct AttractorData {
    Float mass;
    Vector position;
    Float radius;

    /// \todo move elsewhere?
    bool visible;
    Float albedo;
};

struct CurvedRayIntersectionInfo : public IntersectionInfo {
    RaySegment* segment;

    CurvedRayIntersectionInfo() = default;

    CurvedRayIntersectionInfo(const IntersectionInfo& is, RaySegment* segment)
        : IntersectionInfo(is)
        , segment(segment) {}
};

class LensingEffect {
private:
    ArrayView<const AttractorData> attractors;
    Float magnitude;
    Float step;
    Float maxDist;
    bool sort;

public:
    static constexpr Size MAX_STEPS = 20;

    using Segments = StaticArray<RaySegment, MAX_STEPS>;

    LensingEffect(ArrayView<const AttractorData> attractors,
        const Float magnitude,
        const Float step,
        const Float maxDist,
        const bool sort)
        : attractors(attractors)
        , magnitude(magnitude)
        , step(step)
        , maxDist(maxDist)
        , sort(sort) {}

    template <typename TBvhObject>
    Ray getAllIntersections(const Bvh<TBvhObject>& bvh,
        const Ray& ray,
        Segments& segments,
        Array<CurvedRayIntersectionInfo>& intersections) {
        segments.resize(0);
        intersections.clear();
        if (this->needsRayMarch()) {
            return this->rayMarch(ray, [this, &bvh, &segments, &intersections](const RaySegment& segment) {
                Size size0 = intersections.size();
                segments.push(segment);
                bvh.getIntersections(segment, [&segments, &intersections](const IntersectionInfo& is) {
                    intersections.emplaceBack(is, &segments.back());
                    return true;
                });
                if (sort && !intersections.empty()) {
                    std::sort(intersections.begin() + size0, intersections.end());
                }
            });
        } else {
            segments.push(ray);
            bvh.getIntersections(ray, [&ray, &segments, &intersections](const IntersectionInfo& is) {
                intersections.emplaceBack(is, &segments.back());
                return true;
            });
            if (sort) {
                std::sort(intersections.begin(), intersections.end());
            }
            return ray;
        }
    }

private:
    bool needsRayMarch() const {
        return !attractors.empty() && magnitude > 0;
    }

    template <typename TRayFunc>
    Ray rayMarch(const Ray& primaryRay, const TRayFunc& func) const {
        if (attractors.empty()) {
            func(primaryRay);
            return primaryRay;
        }

        Vector r = primaryRay.origin();
        Vector r0 = r;
        Vector v = Constants::speedOfLight * primaryRay.direction();
        for (Float dist = 0; dist < maxDist; dist += step) {
            Vector f = Vector(0._f);
            for (const AttractorData& a : attractors) {
                const Vector delta = r - a.position;
                if (getSqrLength(delta) < sqr(a.radius)) {
                    // absorbed
                    return Ray(r0, getNormalized(v));
                }
                f += -magnitude * Constants::gravity * a.mass * delta / pow<3>(getLength(delta));
            }
            const Float dt = step / getLength(v);
            v += f * dt;
            r += v * dt;
            const Vector dir = r - r0;
            func(RaySegment(r0, getNormalized(dir), Interval(0, getLength(dir))));
            r0 = r;
        }
        return Ray(r0, getNormalized(v));
    }
};

NAMESPACE_SPH_END
