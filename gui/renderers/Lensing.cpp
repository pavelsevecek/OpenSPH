#include "gui/renderers/Lensing.h"
#include "physics/Constants.h"

NAMESPACE_SPH_BEGIN

HyperbolicRay HyperbolicRay::getFromRay(const Ray& ray,
    ArrayView<const AttractorData> attractors,
    const Float magnitude,
    const Float step,
    const Float maxDist) {
    HyperbolicRay hr;
    Vector r = ray.origin();
    Vector r0 = r;
    Vector v = 1.e3_f * ray.direction();
    for (Float dist = 0; dist < maxDist; dist += step) {
        Vector f = Vector(0._f);
        for (const AttractorData& a : attractors) {
            const Vector delta = r - a.position;
            if (getSqrLength(delta) < sqr(a.radius)) {
                // absorbed
                return hr;
            }
            f += -magnitude * Constants::gravity * a.mass * delta / pow<3>(getLength(delta));
        }
        const Float dt = step / getLength(v);
        v += f * dt;
        r += v * dt;
        const Vector dir = r - r0;
        hr.segments.emplaceBack(r0, getNormalized(dir), Interval(0, getLength(dir)));
        r0 = r;
    }
    return hr;
}

NAMESPACE_SPH_END
