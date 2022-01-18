#pragma once

#include "objects/finders/Bvh.h"
#include "quantities/Attractor.h"

NAMESPACE_SPH_BEGIN

struct AttractorData {
    Float mass;
    Vector position;
    Float radius;
};

class HyperbolicRay {
    Array<RaySegment> segments;

public:
    static HyperbolicRay getFromRay(const Ray& ray,
        ArrayView<const AttractorData> attractors,
        const Float magnitude,
        const Float step,
        const Float maxDist);

    const Array<RaySegment>& getSegments() const {
        return segments;
    }
};


NAMESPACE_SPH_END
