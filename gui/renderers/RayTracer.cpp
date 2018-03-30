#include "gui/renderers/RayTracer.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "objects/finders/KdTree.h"
#include "system/Profiler.h"
#include <wx/dcmemory.h>

NAMESPACE_SPH_BEGIN

RayTracer::RayTracer(const GuiSettings& settings)
    : threadData(pool) {
    kernel = Factory::getKernel<3>(RunSettings::getDefaults());
    params.surfaceLevel = settings.get<Float>(GuiSettingsId::SURFACE_LEVEL);
    params.dirToSun = settings.get<Vector>(GuiSettingsId::SURFACE_SUN_POSITION);
    params.brdf = Factory::getBrdf(settings);
}

RayTracer::~RayTracer() = default;

void RayTracer::initialize(const Storage& storage,
    const IColorizer& colorizer,
    const ICamera& UNUSED(camera)) {
    MEASURE_SCOPE("Building BVH");
    cached.r = storage.getValue<Vector>(QuantityId::POSITION).clone();
    const Size particleCnt = cached.r.size();

    cached.flags.resize(particleCnt);
    ArrayView<const Size> idxs = storage.getValue<Size>(QuantityId::FLAG);
    ArrayView<const Float> reduce = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
    // assign separate flag to fully damaged particles so that they do not blend with other particles
    Size damagedIdx = 5;
    for (Size i = 0; i < particleCnt; ++i) {
        if (reduce[i] == 0._f) {
            cached.flags[i] = damagedIdx++;
        } else {
            cached.flags[i] = idxs[i];
        }
    }

    ArrayView<const Float> rho, m;
    tie(rho, m) = storage.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS);
    cached.v.resize(particleCnt);
    for (Size i = 0; i < particleCnt; ++i) {
        cached.v[i] = m[i] / 2700._f; //  rho[i];
    }

    cached.colors.resize(particleCnt);
    for (Size i = 0; i < particleCnt; ++i) {
        cached.colors[i] = colorizer.evalColor(i);
    }

    Array<BvhSphere> spheres;
    spheres.reserve(particleCnt);
    for (Size i = 0; i < particleCnt; ++i) {
        BvhSphere& s = spheres.emplaceBack(cached.r[i], /*2.f * */ cached.r[i][H]);
        s.userData = i;
    }
    bvh.build(std::move(spheres));

    finder = makeAuto<KdTree>();
    finder->build(cached.r);

    threadData.forEach([](ThreadData& data) { data.previousIdx = Size(-1); });
}

static void drawText(wxBitmap& bitmap, const std::string& text) {
    wxMemoryDC dc(bitmap);
    wxFont font = dc.GetFont();
    font.MakeSmaller();
    dc.SetFont(font);

    dc.SetTextForeground(*wxWHITE);
    dc.DrawText(text, 10, 10);

    dc.SelectObject(wxNullBitmap);
}

SharedPtr<wxBitmap> RayTracer::render(const ICamera& camera,
    const RenderParams& params,
    Statistics& UNUSED(stats)) const {
    MEASURE_SCOPE("Rendering frame");
    Bitmap bitmap(params.size);
    /// \todo this currently works only for ortho camera
    const Float ratio =
        getLength(camera.unproject(Point(1, 0)).origin - camera.unproject(Point(0, 0)).origin);
    ASSERT(ratio > 0._f);
    Timer timer;
    parallelFor(pool, 0, Size(params.size.y), [this, &bitmap, &camera, &params, ratio](Size y) {
        ThreadData& data = threadData.get();
        for (Size x = 0; x < Size(params.size.x); ++x) {
            CameraRay cameraRay = camera.unproject(Point(x, y));
            const Ray ray(cameraRay.origin, getNormalized(cameraRay.target - cameraRay.origin));
            bvh.getAllIntersections(ray, data.intersections);
            Color accumulatedColor = Color::black();
            for (const IntersectionInfo& intersect : data.intersections) {
                ShadeContext sc;
                sc.index = intersect.object->userData;
                sc.ray = ray;
                sc.t_min = intersect.t;
                this->getNeighbourList(data, sc.index);
                const Optional<Vector> hit = this->getSurface(data, sc);
                if (hit) {
                    accumulatedColor = this->shade(data, hit.value(), sc);
                    break;
                }
                // rejected, process another intersection
            }
            bitmap[Point(x, y)] = wxColour(accumulatedColor);
        }
    });
    SharedPtr<wxBitmap> wx = makeShared<wxBitmap>();
    toWxBitmap(bitmap, *wx);
    drawText(*wx, "Rendering took " + std::to_string(timer.elapsed(TimerUnit::MILLISECOND)) + "ms");
    return wx;
}

ArrayView<const Size> RayTracer::getNeighbourList(ThreadData& data, const Size index) const {
    // look for neighbours only if the intersected particle differs from the previous one
    if (index != data.previousIdx) {
        Array<NeighbourRecord> neighs;
        finder->findAll(index, 2._f * cached.r[index][H], neighs);
        data.previousIdx = index;

        // find the actual list of neighbours
        data.neighs.clear();
        for (NeighbourRecord& n : neighs) {
            if (cached.flags[index] == cached.flags[n.index]) {
                data.neighs.push(n.index);
            }
        }
    }
    return data.neighs;
}

Optional<Vector> RayTracer::getSurface(ThreadData& data, const ShadeContext& context) const {
    /*if (true) {
        return context.ray.origin() + context.ray.direction() * context.t_min;
    }*/
    const Size i = context.index;
    const Ray& ray = context.ray;
    ASSERT(getSqrLength(ray.direction()) == 1._f, getSqrLength(ray.direction()));
    Vector v1 = ray.origin() + ray.direction() * context.t_min;
    // the sphere hit should be always above the surface
    ASSERT(this->evalField(data.neighs, v1) < 0._f);
    // look for the intersection up to hit + 4H; if we don't find it, we should reject the hit and look for
    // the next intersection - the surface can be non-convex!!
    const Float limit = 2._f * cached.r[i][H];
    // initial step - cannot be too large otherwise the ray could 'tunnel through' on grazing angles
    Float eps = 0.5_f * cached.r[i][H];
    Vector v2 = v1 + eps * ray.direction();

    Float phi = 0._f;
    Float travelled = eps;
    while (travelled < limit && eps > 0.2_f * cached.r[i][H]) {
        phi = this->evalField(data.neighs, v2);
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
        return NOTHING;
    } else {
        return v2;
    }
}

Color RayTracer::shade(ThreadData& data, const Vector& hit, const ShadeContext& context) const {
    // "sample light" - occlusion ray to sun
    // Ray rayToSun(hit .dirToSun * 0.01_f, params.dirToSun);
    /*if (bvh.isOccluded(rayToSun)) {
        /// \todo ambient light?
        return Color::black();
    }*/

    // compute normal = gradient of the field
    /*const Float phi = this->evalField(data.neighs, hit);
    const Float phi_dx = this->evalField(data.neighs, hit + Vector(ratio, 0._f, 0._f));
    const Float phi_dy = this->evalField(data.neighs, hit + Vector(0._f, ratio, 0._f));
    const Float phi_dz = this->evalField(data.neighs, hit + Vector(0._f, 0._f, ratio));*/
    const Vector n = this->evalGradient(data.neighs, hit); // (phi_dx - phi, phi_dy - phi, phi_dz - phi);
    ASSERT(n != Vector(0._f));
    const Vector n_norm = getNormalized(n);
    const Float cosPhi = dot(n_norm, params.dirToSun);
    if (cosPhi <= 0._f) {
        // not illuminated
        return Color::black();
    }
    const Float f = params.brdf->transport(n_norm, -context.ray.direction(), params.dirToSun);
    return this->evalColor(data.neighs, hit) * PI * f * cosPhi;
}

Float RayTracer::evalField(ArrayView<const Size> neighs, const Vector& pos1) const {
    ASSERT(!neighs.empty());
    Float value = 0._f;
    for (Size index : neighs) {
        const Vector& pos2 = cached.r[index];
        /// \todo could be optimized by using n.distSqr, no need to compute the dot again
        const Float w = kernel.value(pos1 - pos2, pos2[H]);
        value += cached.v[index] * w;
    }
    return value - params.surfaceLevel;
}

Vector RayTracer::evalGradient(ArrayView<const Size> neighs, const Vector& pos1) const {
    Vector value(0._f);
    for (Size index : neighs) {
        const Vector& pos2 = cached.r[index];
        const Vector grad = kernel.grad(pos1 - pos2, pos2[H]);
        value += cached.v[index] * grad;
    }
    return value;
}

Color RayTracer::evalColor(ArrayView<const Size> neighs, const Vector& pos1) const {
    ASSERT(!neighs.empty());
    Color color = Color::black();
    Float weightSum = 0._f;
    for (Size index : neighs) {
        const Vector& pos2 = cached.r[index];
        /// \todo could be optimized by using n.distSqr, no need to compute the dot again
        const Float w = kernel.value(pos1 - pos2, pos2[H]) * cached.v[index];
        color += cached.colors[index] * w;
        weightSum += w;
    }
    ASSERT(weightSum != 0._f);
    return color / weightSum;
}

NAMESPACE_SPH_END
