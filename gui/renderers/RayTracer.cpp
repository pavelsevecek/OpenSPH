#include "gui/renderers/RayTracer.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "objects/finders/KdTree.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

RayTracer::RayTracer(const GuiSettings& settings) {
    kernel = Factory::getKernel<3>(RunSettings::getDefaults());
    params.surfaceLevel = settings.get<Float>(GuiSettingsId::SURFACE_LEVEL);
}

void RayTracer::initialize(const Storage& storage,
    const IColorizer& colorizer,
    const ICamera& UNUSED(camera)) {
    MEASURE_SCOPE("Building BVH");
    cached.r = storage.getValue<Vector>(QuantityId::POSITION).clone();
    const Size particleCnt = cached.r.size();

    cached.idxs = storage.getValue<Size>(QuantityId::FLAG).clone();

    ArrayView<const Float> rho, m;
    tie(rho, m) = storage.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS);
    cached.v.resize(particleCnt);
    for (Size i = 0; i < particleCnt; ++i) {
        cached.v[i] = m[i] / rho[i];
    }

    cached.colors.resize(particleCnt);
    for (Size i = 0; i < particleCnt; ++i) {
        cached.colors[i] = colorizer.evalColor(i);
    }

    Array<BvhSphere> spheres;
    spheres.reserve(particleCnt);
    for (Size i = 0; i < particleCnt; ++i) {
        /// \todo hardcoded for r_kernel = 2
        BvhSphere& s = spheres.emplaceBack(cached.r[i], 2._f * cached.r[i][H]);
        s.userData = i;
    }
    bvh.build(std::move(spheres));

    finder = makeAuto<KdTree>();
    finder->build(cached.r);

    cached.previousIdx = Size(-1);
}

SharedPtr<Bitmap> RayTracer::render(const ICamera& camera,
    const RenderParams& params,
    Statistics& UNUSED(stats)) const {
    MEASURE_SCOPE("Rendering frame");
    IntersectionInfo intersection;
    SharedPtr<Bitmap> bitmap = makeShared<Bitmap>(params.size);
    BitmapIterator iterator(*bitmap);

    for (Size y = 0; y < Size(params.size.y); ++y) {
        for (Size x = 0; x < Size(params.size.x); ++x) {
            CameraRay cameraRay = camera.unproject(Point(x, y));
            Ray ray(cameraRay.origin, getNormalized(cameraRay.target - cameraRay.origin));
            if (bvh.getIntersection(ray, intersection)) {
                /// \todo currently cannot handle non-convex surfaces !!!
                const Color color = this->shade(intersection.object->userData, ray, intersection.t);
                iterator.setColor(color);
            } else {
                iterator.setColor(Color::black());
            }
            ++iterator;
        }
    }
    return bitmap;
}

Color RayTracer::shade(const Size i, const Ray& ray, const Float t_min) const {
    // look for neighbours only if the intersected particle differs from the previous one
    if (i != cached.previousIdx) {
        finder->findAll(i, 2._f * cached.r[i][H], cached.neighs);
        cached.previousIdx = i;
    }

    ASSERT(getSqrLength(ray.direction()) == 1._f, getSqrLength(ray.direction()));
    Vector v1 = ray.origin() + ray.direction() * t_min;
    const Float flag1 = cached.idxs[i];
    ASSERT(this->eval(v1, flag1) < 0._f); // the sphere hit should be always above the surface
    // look for the intersection up to hit + 2H; if we don't find it, we should reject the hit and look for
    // the next intersection - the surface can be non-convex!!
    const Float limit = 2._f * cached.r[i][H];
    // initial step
    Float eps = 0.3_f * cached.r[i][H];
    Vector v2 = v1 + eps * ray.direction();

    Float travelled = eps;
    while (travelled < limit && eps > 0.03 * cached.r[i][H]) {
        const Float phi = this->eval(v2, flag1);
        if (phi > 0._f) {
            // we crossed the surface, move back
            v2 = 0.5_f * (v1 + v2);
            eps *= 0.5_f;
            // since we crossed the surface, don't check for travelled distance anymore
            travelled = -INFTY;
        } else {
            // we are still above the surface, move further
            v1 = v2;
            v2 += eps * ray.direction();
            travelled += eps;
        }
    }

    if (travelled >= limit) {
        // didn't find surface, reject the hit
        /// \todo
        return Color::black();
    } else {
        return Color::white();
    }
}

Float RayTracer::eval(const Vector& pos1, const Size flag1) const {
    ASSERT(!cached.neighs.empty());
    Float value = 0._f;
    for (const NeighbourRecord& n : cached.neighs) {
        if (flag1 != cached.idxs[n.index]) {
            // don't interpolate between different bodies
            continue;
        }
        const Vector& pos2 = cached.r[n.index];
        /// \todo could be optimized by using n.distSqr, no need to compute the dot again
        const Float w = kernel.value(pos1 - pos2, pos2[H]);
        value += cached.v[n.index] * w;
    }
    ASSERT(value > 0._f);
    return value - params.surfaceLevel;
}

NAMESPACE_SPH_END
