#include "gui/renderers/RayMarcher.h"
#include "gui/Factory.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/FrameBuffer.h"
#include "objects/containers/FlatMap.h"
#include "objects/utility/OutputIterators.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

RayMarcher::RayMarcher(SharedPtr<IScheduler> scheduler, const GuiSettings& settings)
    : IRaytracer(scheduler, settings) {
    kernel = CubicSpline<3>();
    fixed.dirToSun = getNormalized(settings.get<Vector>(GuiSettingsId::SURFACE_SUN_POSITION));
    fixed.brdf = Factory::getBrdf(settings);
    fixed.renderSpheres = settings.get<bool>(GuiSettingsId::RAYTRACE_SPHERES);
    fixed.shadows = settings.get<bool>(GuiSettingsId::RAYTRACE_SHADOWS);
}

RayMarcher::~RayMarcher() = default;

constexpr Size BLEND_ALL_FLAG = 0x80;

void RayMarcher::initialize(const Storage& storage,
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

    cached.materialIDs.resize(particleCnt);
    cached.materialIDs.fill(0);
    const bool loadTextures = cached.textures.empty();
    if (loadTextures) {
        cached.textures.resize(storage.getMaterialCnt());
    }
    FlatMap<String, SharedPtr<Texture>> textureMap;
    for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
        MaterialView body = storage.getMaterial(matId);
        for (Size i : body.sequence()) {
            cached.materialIDs[i] = matId;
        }

        String texturePath = body->getParam<String>(BodySettingsId::VISUALIZATION_TEXTURE);
        if (loadTextures && !texturePath.empty()) {
            if (textureMap.contains(texturePath)) {
                cached.textures[matId] = textureMap[texturePath];
            } else {
                SharedPtr<Texture> texture =
                    makeShared<Texture>(Path(texturePath), TextureFiltering::BILINEAR);
                textureMap.insert(texturePath, texture);
                cached.textures[matId] = texture;
            }
        }
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

    this->setColorizer(colorizer);

    Array<BvhSphere> spheres;
    spheres.reserve(particleCnt);
    for (Size i = 0; i < particleCnt; ++i) {
        BvhSphere& s = spheres.emplaceBack(cached.r[i], /*2.f * */ cached.r[i][H]);
        s.userData = i;
    }
    bvh.build(std::move(spheres));

    finder = Factory::getFinder(RunSettings::getDefaults());
    finder->build(*scheduler, cached.r);

    for (ThreadData& data : threadData) {
        MarchData march;
        march.previousIdx = Size(-1);
        data.data = std::move(march);
    }

    shouldContinue = true;
}

bool RayMarcher::isInitialized() const {
    return !cached.r.empty();
}

void RayMarcher::setColorizer(const IColorizer& colorizer) {
    cached.doEmission = false;
    cached.colors.resize(cached.r.size());
    for (Size i = 0; i < cached.r.size(); ++i) {
        cached.colors[i] = colorizer.evalColor(i);
        if (cached.doEmission) {
            cached.colors[i] = cached.colors[i] * colorizer.evalScalar(i).value();
        }
    }
}

Rgba RayMarcher::shade(const RenderParams& params, const CameraRay& cameraRay, ThreadData& data) const {
    const Vector dir = getNormalized(cameraRay.target - cameraRay.origin);
    const Ray ray(cameraRay.origin, dir);

    MarchData& march(data.data);
    if (Optional<Vector> hit = this->intersect(march, ray, params.surface.level, false)) {
        return this->getSurfaceColor(march, params, march.previousIdx, hit.value(), ray.direction());
    } else {
        return this->getEnviroColor(cameraRay);
    }
}

ArrayView<const Size> RayMarcher::getNeighborList(MarchData& data, const Size index) const {
    // look for neighbors only if the intersected particle differs from the previous one
    if (index != data.previousIdx) {
        Array<NeighborRecord> neighs;
        finder->findAll(index, kernel.radius() * cached.r[index][H], neighs);
        data.previousIdx = index;

        // find the actual list of neighbors
        data.neighs.clear();
        for (NeighborRecord& n : neighs) {
            const Size flag1 = cached.flags[index];
            const Size flag2 = cached.flags[n.index];
            if ((flag1 & BLEND_ALL_FLAG) || (flag2 & BLEND_ALL_FLAG) || (flag1 == flag2)) {
                data.neighs.push(n.index);
            }
        }
    }
    return data.neighs;
}

Optional<Vector> RayMarcher::intersect(MarchData& data,
    const Ray& ray,
    const Float surfaceLevel,
    const bool occlusion) const {
    data.intersections.clear();
    bvh.getAllIntersections(ray, backInserter(data.intersections));
    std::sort(data.intersections.begin(), data.intersections.end());

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

Optional<Vector> RayMarcher::getSurfaceHit(MarchData& data,
    const IntersectContext& context,
    bool occlusion) const {
    if (fixed.renderSpheres) {
        data.previousIdx = context.index;
        return context.ray.origin() + context.ray.direction() * context.t_min;
    }

    this->getNeighborList(data, context.index);

    const Size i = context.index;
    const Ray& ray = context.ray;
    SPH_ASSERT(almostEqual(getSqrLength(ray.direction()), 1._f), getSqrLength(ray.direction()));
    Vector v1 = ray.origin() + ray.direction() * context.t_min;
    // the sphere hit should be always above the surface
    // SPH_ASSERT(this->evalField(data.neighs, v1) < 0._f);
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

Rgba RayMarcher::getSurfaceColor(MarchData& data,
    const RenderParams& params,
    const Size index,
    const Vector& hit,
    const Vector& dir) const {
    Rgba diffuse = Rgba::white();
    if (!cached.textures.empty() && !cached.uvws.empty()) {
        Size textureIdx = cached.materialIDs[index];
        SPH_ASSERT(textureIdx <= 10); // just sanity check, increase if necessary
        if (textureIdx >= cached.textures.size()) {
            textureIdx = 0;
        }
        if (cached.textures[textureIdx]) {
            const Vector uvw = this->evalUvws(data.neighs, hit);
            diffuse = cached.textures[textureIdx]->eval(uvw);
        }
    }

    // evaluate color before checking for occlusion as that invalidates the neighbor list
    const Rgba colorizerValue =
        fixed.renderSpheres ? cached.colors[index] : this->evalColor(data.neighs, hit);

    Rgba emission = Rgba::black();
    if (cached.doEmission) {
        emission = colorizerValue * params.surface.emission;
    } else {
        diffuse = diffuse * colorizerValue;
    }

    // compute the inward normal = gradient of the field
    const Vector n = fixed.renderSpheres ? cached.r[index] - hit : this->evalGradient(data.neighs, hit);
    SPH_ASSERT(n != Vector(0._f));
    const Vector n_norm = getNormalized(n);
    const Float cosPhi = dot(n_norm, fixed.dirToSun);
    if (cosPhi <= 0._f) {
        // not illuminated -> just ambient light + emission
        return diffuse * params.surface.ambientLight + emission;
    }

    // check for occlusion
    if (fixed.shadows) {
        Ray rayToSun(hit - 0.5_f * n_norm * cached.r[index][H], -fixed.dirToSun);
        if (this->intersect(data, rayToSun, params.surface.level, true)) {
            // casted shadow
            return diffuse * params.surface.ambientLight + emission;
        }
    }

    // evaluate BRDF
    const Float f = fixed.brdf->transport(n_norm, -dir, fixed.dirToSun);

    return diffuse * float(PI * f * cosPhi * params.surface.sunLight + params.surface.ambientLight) +
           emission;
}

Float RayMarcher::evalField(ArrayView<const Size> neighs, const Vector& pos1) const {
    SPH_ASSERT(!neighs.empty());
    Float value = 0._f;
    for (Size index : neighs) {
        const Vector& pos2 = cached.r[index];
        /// \todo could be optimized by using n.distSqr, no need to compute the dot again
        const Float w = kernel.value(pos1 - pos2, pos2[H]);
        value += cached.v[index] * w;
    }
    return value;
}

Vector RayMarcher::evalGradient(ArrayView<const Size> neighs, const Vector& pos1) const {
    Vector value(0._f);
    for (Size index : neighs) {
        const Vector& pos2 = cached.r[index];
        const Vector grad = kernel.grad(pos1 - pos2, pos2[H]);
        value += cached.v[index] * grad;
    }
    return value;
}

Rgba RayMarcher::evalColor(ArrayView<const Size> neighs, const Vector& pos1) const {
    SPH_ASSERT(!neighs.empty());
    Rgba color = Rgba::black();
    float weightSum = 0.f;
    for (Size index : neighs) {
        const Vector& pos2 = cached.r[index];
        /// \todo could be optimized by using n.distSqr, no need to compute the dot again
        const float w = float(kernel.value(pos1 - pos2, pos2[H]) * cached.v[index]);
        color += cached.colors[index] * w;
        weightSum += w;
    }
    SPH_ASSERT(weightSum != 0._f);
    return color / weightSum;
}

constexpr Float SEAM_WIDTH = 0.1_f;

Vector RayMarcher::evalUvws(ArrayView<const Size> neighs, const Vector& pos1) const {
    SPH_ASSERT(!neighs.empty());
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
        SPH_ASSERT(weightSum != 0._f);
        uvws /= weightSum;
        uvws[X] += (uvws[X] < 0._f) ? 1._f : 0._f;
        return uvws;
    } else {
        SPH_ASSERT(weightSum != 0._f);
        return uvws / weightSum;
    }
}

NAMESPACE_SPH_END
