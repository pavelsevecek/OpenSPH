#include "gui/renderers/RayTracer.h"
#include "gui/Factory.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/FrameBuffer.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

RayTracer::RayTracer(IScheduler& scheduler, const GuiSettings& settings)
    : scheduler(scheduler)
    , threadData(scheduler) {
    kernel = CubicSpline<3>();
    fixed.dirToSun = settings.get<Vector>(GuiSettingsId::SURFACE_SUN_POSITION);
    fixed.brdf = Factory::getBrdf(settings);
    fixed.subsampling = settings.get<int>(GuiSettingsId::RAYTRACE_SUBSAMPLING);
    fixed.iterationLimit = settings.get<int>(GuiSettingsId::RAYTRACE_ITERATION_LIMIT);

    fixed.enviro.color = settings.get<Rgba>(GuiSettingsId::BACKGROUND_COLOR);
    std::string hdriPath = settings.get<std::string>(GuiSettingsId::RAYTRACE_HDRI);
    if (!hdriPath.empty()) {
        fixed.enviro.hdri = Texture(Path(hdriPath), TextureFiltering::BILINEAR);
    }

    std::string primaryPath = settings.get<std::string>(GuiSettingsId::RAYTRACE_TEXTURE_PRIMARY);
    if (!primaryPath.empty()) {
        fixed.textures.emplaceBack(Path(primaryPath), TextureFiltering::BILINEAR);

        std::string secondaryPath = settings.get<std::string>(GuiSettingsId::RAYTRACE_TEXTURE_SECONDARY);
        if (!secondaryPath.empty()) {
            fixed.textures.emplaceBack(Path(secondaryPath), TextureFiltering::BILINEAR);
        } else {
            fixed.textures.emplaceBack(fixed.textures.front().clone());
        }
    }

    shouldContinue = true;
}

RayTracer::~RayTracer() = default;

constexpr Size BLEND_ALL_FLAG = 0x80;

void RayTracer::initialize(const Storage& storage,
    const IColorizer& colorizer,
    const ICamera& UNUSED(camera)) {
    MEASURE_SCOPE("Building BVH");
    cached.r = storage.getValue<Vector>(QuantityId::POSITION).clone();
    const Size particleCnt = cached.r.size();

    if (storage.has(QuantityId::UVW)) {
        cached.uvws = storage.getValue<Vector>(QuantityId::UVW).clone();
    } else {
        cached.uvws.clear();
    }

    cached.flags.resize(particleCnt);
    if (storage.has(QuantityId::FLAG) && storage.has(QuantityId::STRESS_REDUCING)) {
        ArrayView<const Size> idxs = storage.getValue<Size>(QuantityId::FLAG);
        ArrayView<const Float> reduce = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
        // avoid blending particles of different bodies, except if they are fully damaged
        for (Size i = 0; i < particleCnt; ++i) {
            cached.flags[i] = idxs[i];
            if (reduce[i] == 0._f) {
                cached.flags[i] |= BLEND_ALL_FLAG;
            }
        }
    } else {
        cached.flags.fill(0);
    }

    cached.v.resize(particleCnt);
    if (storage.has(QuantityId::MASS) && storage.has(QuantityId::DENSITY)) {
        ArrayView<const Float> rho, m;
        tie(rho, m) = storage.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS);
        for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
            MaterialView material = storage.getMaterial(matId);
            const Float rho = material->getParam<Float>(BodySettingsId::DENSITY);
            for (Size i : material.sequence()) {
                cached.v[i] = m[i] / rho;
            }
        }
    } else {
        for (Size i = 0; i < particleCnt; ++i) {
            cached.v[i] = sphereVolume(cached.r[i][H]);
        }
    }

    cached.colors.resize(particleCnt);
    for (Size i = 0; i < particleCnt; ++i) {
        cached.colors[i] = colorizer.evalColor(i);
    }
    cached.doEmission = typeid(colorizer) == typeid(BeautyColorizer);

    Array<BvhSphere> spheres;
    spheres.reserve(particleCnt);
    for (Size i = 0; i < particleCnt; ++i) {
        BvhSphere& s = spheres.emplaceBack(cached.r[i], /*2.f * */ cached.r[i][H]);
        s.userData = i;
    }
    bvh.build(std::move(spheres));

    finder = Factory::getFinder(RunSettings::getDefaults());
    finder->build(scheduler, cached.r);

    for (ThreadData& data : threadData) {
        data.previousIdx = Size(-1);
    }

    shouldContinue = true;
}

void RayTracer::render(const RenderParams& params, Statistics& UNUSED(stats), IRenderOutput& output) const {
    shouldContinue = true;
    ASSERT(finder && bvh.getBoundingBox().volume() > 0._f);
    FrameBuffer fb(params.size);
    for (Size iteration = 0; iteration < fixed.iterationLimit && shouldContinue; ++iteration) {
        this->refine(params, iteration, fb);
        output.update(fb.bitmap(), {});
    }
}

void RayTracer::refine(const RenderParams& params, const Size iteration, FrameBuffer& fb) const {
    MEASURE_SCOPE("Rendering frame");
    const Size level = 1 << max(int(fixed.subsampling) - int(iteration), 0);
    Pixel actSize;
    actSize.x = params.size.x / level + sgn(params.size.x % level);
    actSize.y = params.size.y / level + sgn(params.size.y % level);
    Bitmap<Rgba> bitmap(actSize);

    const bool first = (iteration == 0);
    parallelFor(scheduler,
        threadData,
        0,
        Size(bitmap.size().y),
        [this, &bitmap, &params, level, first](Size y, ThreadData& data) {
            if (!shouldContinue && !first) {
                return;
            }
            for (Size x = 0; x < Size(bitmap.size().x); ++x) {
                /// \todo generalize
                const float rx = x * level + (level == 1 ? data.rng() : 0.5f);
                const float ry = y * level + (level == 1 ? data.rng() : 0.5f);
                CameraRay cameraRay = params.camera->unproject(Coords(rx, ry));
                const Vector dir = getNormalized(cameraRay.target - cameraRay.origin);
                const Ray ray(cameraRay.origin, dir);

                Rgba accumulatedColor = Rgba::transparent();
                if (Optional<Vector> hit = this->intersect(data, ray, params.surface.level, false)) {
                    accumulatedColor =
                        this->shade(data, params, data.previousIdx, hit.value(), ray.direction());
                } else {
                    accumulatedColor = this->getEnviroColor(ray);
                }
                bitmap[Pixel(x, y)] = accumulatedColor;
            }
        });

    if (!shouldContinue && !first) {
        return;
    }
    if (level == 1) {
        fb.accumulate(bitmap);
    } else {
        Bitmap<Rgba> full(params.size);
        for (Size y = 0; y < Size(full.size().y); ++y) {
            for (Size x = 0; x < Size(full.size().x); ++x) {
                full[Pixel(x, y)] = bitmap[Pixel(x / level, y / level)];
            }
        }
        fb.override(std::move(full));
    }
}

ArrayView<const Size> RayTracer::getNeighbourList(ThreadData& data, const Size index) const {
    // look for neighbours only if the intersected particle differs from the previous one
    if (index != data.previousIdx) {
        Array<NeighbourRecord> neighs;
        finder->findAll(index, kernel.radius() * cached.r[index][H], neighs);
        data.previousIdx = index;

        // find the actual list of neighbours
        data.neighs.clear();
        for (NeighbourRecord& n : neighs) {
            const Size flag1 = cached.flags[index];
            const Size flag2 = cached.flags[n.index];
            if ((flag1 & BLEND_ALL_FLAG) || (flag2 & BLEND_ALL_FLAG) || (flag1 == flag2)) {
                data.neighs.push(n.index);
            }
        }
    }
    return data.neighs;
}

Optional<Vector> RayTracer::intersect(ThreadData& data,
    const Ray& ray,
    const Float surfaceLevel,
    const bool occlusion) const {
    bvh.getAllIntersections(ray, data.intersections);

    for (const IntersectionInfo& intersect : data.intersections) {
        IntersectContext sc;
        sc.index = intersect.object->userData;
        sc.ray = ray;
        sc.t_min = intersect.t;
        sc.surfaceLevel = surfaceLevel;
        const Optional<Vector> hit = this->getSurfaceHit(data, sc, occlusion);
        if (hit) {
            return hit;
        }
        // rejected, process another intersection
    }
    return NOTHING;
}

Optional<Vector> RayTracer::getSurfaceHit(ThreadData& data,
    const IntersectContext& context,
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
        phi = this->evalField(data.neighs, v2) - context.surfaceLevel;
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

Rgba RayTracer::shade(ThreadData& data,
    const RenderParams& params,
    const Size index,
    const Vector& hit,
    const Vector& dir) const {
    Rgba color = Rgba::white();
    if (!fixed.textures.empty() && !cached.uvws.empty()) {
        Size textureIdx = cached.flags[index] & ~BLEND_ALL_FLAG;
        ASSERT(textureIdx <= 10); // just sanity check, increase if necessary
        if (textureIdx >= fixed.textures.size()) {
            textureIdx = 0;
        }
        const Vector uvw = this->evalUvws(data.neighs, hit);
        color = fixed.textures[textureIdx].eval(uvw) * 2._f;
    }

    // evaluate color before checking for occlusion as that invalidates the neighbour list
    const Rgba colorizerValue = this->evalColor(data.neighs, hit) * color;

    // compute normal = gradient of the field
    const Vector n = this->evalGradient(data.neighs, hit);
    ASSERT(n != Vector(0._f));
    const Vector n_norm = getNormalized(n);
    const Float cosPhi = dot(n_norm, fixed.dirToSun);
    if (cosPhi <= 0._f) {
        // not illuminated
        return colorizerValue * params.surface.ambientLight;
    }

    // check for occlusion
    if (fixed.shadows) {
        Ray rayToSun(hit - 1.e-3_f * fixed.dirToSun, -fixed.dirToSun);
        if (this->intersect(data, rayToSun, params.surface.level, true)) {
            // casted shadow
            return colorizerValue * params.surface.ambientLight;
        }
    }

    // evaluate BRDF
    const Float f = fixed.brdf->transport(n_norm, -dir, fixed.dirToSun);

    return colorizerValue * (PI * f * cosPhi * params.surface.sunLight + params.surface.ambientLight);
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
    return value;
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

Rgba RayTracer::evalColor(ArrayView<const Size> neighs, const Vector& pos1) const {
    ASSERT(!neighs.empty());
    Rgba color = Rgba::black();
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

Rgba RayTracer::getEnviroColor(const Ray& ray) const {
    if (fixed.enviro.hdri.empty()) {
        return fixed.enviro.color;
    } else {
        const Vector dir = ray.direction();
        /// \todo deduplicate with setupUvws
        const SphericalCoords spherical = cartensianToSpherical(Vector(dir[X], dir[Z], dir[Y]));
        const Vector uvw(spherical.phi / (2._f * PI) + 0.5_f, spherical.theta / PI, 0._f);
        return fixed.enviro.hdri.eval(uvw);
    }
}

NAMESPACE_SPH_END
