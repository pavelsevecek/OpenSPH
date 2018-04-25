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
    params.ambientLight = settings.get<Float>(GuiSettingsId::SURFACE_AMBIENT);
    params.subsampling = settings.get<int>(GuiSettingsId::RAYTRACE_SUBSAMPLING);

    std::string hdriPath = settings.get<std::string>(GuiSettingsId::RAYTRACE_HDRI);
    if (!hdriPath.empty()) {
        params.enviro.hdri = Texture(Path(hdriPath), TextureFiltering::BILINEAR);
    }

    std::string primaryPath = settings.get<std::string>(GuiSettingsId::RAYTRACE_TEXTURE_PRIMARY);
    if (!primaryPath.empty()) {
        params.textures.emplaceBack(Path(primaryPath), TextureFiltering::BILINEAR);

        std::string secondaryPath = settings.get<std::string>(GuiSettingsId::RAYTRACE_TEXTURE_SECONDARY);
        if (!secondaryPath.empty()) {
            params.textures.emplaceBack(Path(secondaryPath), TextureFiltering::BILINEAR);
        } else {
            params.textures.emplaceBack(params.textures.front().clone());
        }
    }
}

RayTracer::~RayTracer() = default;

void RayTracer::initialize(const Storage& storage,
    const IColorizer& colorizer,
    const ICamera& UNUSED(camera)) {
    MEASURE_SCOPE("Building BVH");
    cached.r = storage.getValue<Vector>(QuantityId::POSITION).clone();
    const Size particleCnt = cached.r.size();

    if (storage.has(QuantityId(GuiQuantityId::UVW))) {
        cached.uvws = storage.getValue<Vector>(QuantityId(GuiQuantityId::UVW)).clone();
    } else {
        cached.uvws.clear();
    }

    cached.flags.resize(particleCnt);
    ArrayView<const Size> idxs = storage.getValue<Size>(QuantityId::FLAG);
    ArrayView<const Float> reduce = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
    // assign separate flag to fully damaged particles so that they do not blend with other particles
    // Size damagedIdx = 5;
    for (Size i = 0; i < particleCnt; ++i) {
        if (reduce[i] == 0._f) {
            cached.flags[i] = idxs[i]; // damagedIdx++;
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
    const RenderParams& renderParams,
    Statistics& UNUSED(stats)) const {
    MEASURE_SCOPE("Rendering frame");
    Point actSize;
    actSize.x = renderParams.size.x / params.subsampling + sgn(renderParams.size.x % params.subsampling);
    actSize.y = renderParams.size.y / params.subsampling + sgn(renderParams.size.y % params.subsampling);
    Bitmap bitmap(actSize);
    Timer timer;
    parallelFor(pool, 0, Size(bitmap.size().y), [this, &bitmap, &camera, &renderParams](Size y) {
        ThreadData& data = threadData.get();
        for (Size x = 0; x < Size(bitmap.size().x); ++x) {
            CameraRay cameraRay = camera.unproject(Point(x * params.subsampling, y * params.subsampling));
            const Vector dir = getNormalized(cameraRay.target - cameraRay.origin);
            const Ray ray(cameraRay.origin, dir);

            Color accumulatedColor;
            if (Optional<Vector> hit = this->intersect(data, ray, false)) {
                accumulatedColor = this->shade(data, data.previousIdx, hit.value(), ray.direction());
            } else {
                accumulatedColor = this->getEnviroColor(ray);
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

Optional<Vector> RayTracer::intersect(ThreadData& data, const Ray& ray, const bool occlusion) const {
    bvh.getAllIntersections(ray, data.intersections);

    for (const IntersectionInfo& intersect : data.intersections) {
        ShadeContext sc;
        sc.index = intersect.object->userData;
        sc.ray = ray;
        sc.t_min = intersect.t;
        const Optional<Vector> hit = this->getSurfaceHit(data, sc, occlusion);
        if (hit) {
            return hit;
        }
        // rejected, process another intersection
    }
    return NOTHING;
}

Optional<Vector> RayTracer::getSurfaceHit(ThreadData& data,
    const ShadeContext& context,
    bool occlusion) const {
    this->getNeighbourList(data, context.index);

    const Size i = context.index;
    const Ray& ray = context.ray;
    ASSERT(almostEqual(getSqrLength(ray.direction()), 1._f), getSqrLength(ray.direction()));
    Vector v1 = ray.origin() + ray.direction() * context.t_min;
    // the sphere hit should be always above the surface
    // ASSERT(this->evalField(data.neighs, v1) < 0._f);
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
            if (occlusion) {
                return v2;
            }
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

Color RayTracer::shade(ThreadData& data, const Size index, const Vector& hit, const Vector& dir) const {
    Color color = Color::white();
    if (!params.textures.empty() && !cached.uvws.empty()) {
        const Vector uvw = this->evalUvws(data.neighs, hit);
        color = params.textures[cached.flags[index]].eval(uvw) * 2._f;
    }

    // compute normal = gradient of the field
    const Vector n = this->evalGradient(data.neighs, hit);
    ASSERT(n != Vector(0._f));
    const Vector n_norm = getNormalized(n);
    const Float cosPhi = dot(n_norm, params.dirToSun);
    if (cosPhi <= 0._f) {
        // not illuminated
        return Color(params.ambientLight) * color;
    }

    // evaluate color before checking for occlusion as that invalidates the neighbour list
    const Color colorizerValue = this->evalColor(data.neighs, hit);

    // check for occlusion
    if (params.shadows) {
        Ray rayToSun(hit - 1.e-3_f * params.dirToSun, -params.dirToSun);
        if (this->intersect(data, rayToSun, true)) {
            // casted shadow
            return Color(params.ambientLight) * color;
        }
    }

    // evaluate BRDF
    const Float f = params.brdf->transport(n_norm, -dir, params.dirToSun);

    return color * (colorizerValue * PI * f * cosPhi + Color(params.ambientLight));
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

constexpr Float SEAM_WIDTH = 0.1_f;

Vector RayTracer::evalUvws(ArrayView<const Size> neighs, const Vector& pos1) const {
    ASSERT(!neighs.empty());
    Vector uvws(0._f);
    Float weightSum = 0._f;
    int seamFlag = 0;
    for (Size index : neighs) {
        const Vector& pos2 = cached.r[index];
        const Float weight = kernel.value(pos1 - pos2, pos2[H]) * cached.v[index];
        uvws += cached.uvws[index] * weight;
        weightSum += weight;
        seamFlag |= cached.uvws[index][X] < SEAM_WIDTH ? 0x01 : 0;
        seamFlag |= cached.uvws[index][X] > 1._f - SEAM_WIDTH ? 0x02 : 0;
    }
    if (seamFlag & 0x03) {
        // we are near seam in u-coordinate, we cannot interpolate the UVWs directly
        uvws = Vector(0._f);
        weightSum = 0._f;
        for (Size index : neighs) {
            const Vector& pos2 = cached.r[index];
            /// \todo optimize - cache the kernel values
            const Float weight = kernel.value(pos1 - pos2, pos2[H]) * cached.v[index];
            Vector uvw = cached.uvws[index];
            // if near the seam, subtract 1 to make the u-mapping continuous
            uvw[X] -= (uvw[X] > 0.5_f) ? 1._f : 0._f;
            uvws += uvw * weight;
            weightSum += weight;
        }
        ASSERT(weightSum != 0._f);
        uvws /= weightSum;
        uvws[X] += (uvws[X] < 0._f) ? 1._f : 0._f;
        return uvws;
    } else {
        ASSERT(weightSum != 0._f);
        return uvws / weightSum;
    }
}

Color RayTracer::getEnviroColor(const Ray& ray) const {
    if (params.enviro.hdri.empty()) {
        return params.enviro.color;
    } else {
        const Vector dir = ray.direction();
        /// \todo deduplicate with setupUvws
        const SphericalCoords spherical = cartensianToSpherical(Vector(dir[X], dir[Z], dir[Y]));
        const Vector uvw(spherical.phi / (2._f * PI) + 0.5_f, spherical.theta / PI, 0._f);
        return params.enviro.hdri.eval(uvw);
    }
}

NAMESPACE_SPH_END
