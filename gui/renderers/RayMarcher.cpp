#include "gui/renderers/RayMarcher.h"
#include "gui/Factory.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/Shader.h"
#include "gui/renderers/FrameBuffer.h"
#include "objects/containers/FlatMap.h"
#include "objects/finders/KdTree.h"
#include "objects/utility/OutputIterators.h"
#include "quantities/Attractor.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

inline auto seeder() {
    return [seed = 1337]() mutable { return seed++; };
}

Raytracer::ThreadData::ThreadData(const int seed)
    : rng(seed) {}

Raytracer::Raytracer(SharedPtr<IScheduler> scheduler, const GuiSettings& settings)
    : scheduler(scheduler)
    , threadData(*scheduler, seeder()) {
    kernel = CubicSpline<3>();

    fixed.colorMap = Factory::getColorMap(settings);
    fixed.subsampling = settings.get<int>(GuiSettingsId::RAYTRACE_SUBSAMPLING);
    fixed.iterationLimit = settings.get<int>(GuiSettingsId::RAYTRACE_ITERATION_LIMIT);

    fixed.enviro.color = settings.get<Rgba>(GuiSettingsId::BACKGROUND_COLOR);
    String hdriPath = settings.get<String>(GuiSettingsId::RAYTRACE_HDRI);
    if (!hdriPath.empty()) {
        fixed.enviro.hdri = Texture(Path(hdriPath), TextureFiltering::BILINEAR);
    }

    fixed.dirToSun = getNormalized(settings.get<Vector>(GuiSettingsId::SURFACE_SUN_POSITION));
    fixed.maxDistention = settings.get<Float>(GuiSettingsId::VOLUME_MAX_DISTENTION);
    // fixed.brdf = Factory::getBrdf(settings);
    fixed.shadows = settings.get<bool>(GuiSettingsId::RAYTRACE_SHADOWS);

    shouldContinue = true;
}

Raytracer::~Raytracer() = default;

void Raytracer::render(const RenderParams& params, Statistics& UNUSED(stats), IRenderOutput& output) const {
    shouldContinue = true;

    if (RawPtr<LogarithmicColorMap> logMap = dynamicCast<LogarithmicColorMap>(fixed.colorMap.get())) {
        logMap->setFactor(params.post.compressionFactor);
    }

    FrameBuffer fb(params.camera->getSize());
    for (Size iteration = 0; iteration < fixed.iterationLimit && shouldContinue; ++iteration) {
        this->refine(params, iteration, fb);

        const bool isFinal = (iteration == fixed.iterationLimit - 1);
        postProcess(fb, params, isFinal, output);
    }
}

INLINE float sampleTent(const float x) {
    if (x < 0.5f) {
        return sqrt(2.f * x) - 1.f;
    } else {
        return 1.f - sqrt(1.f - 2.f * (x - 0.5f));
    }
}

INLINE Coords sampleTent2d(const Size level, const float halfWidth, UniformRng& rng) {
    if (level == 1) {
        const float x = 0.5f + sampleTent(float(rng())) * halfWidth;
        const float y = 0.5f + sampleTent(float(rng())) * halfWidth;
        return Coords(x, y);
    } else {
        // center of the pixel
        return Coords(0.5f, 0.5f);
    }
}

void Raytracer::refine(const RenderParams& params, const Size iteration, FrameBuffer& fb) const {
    MEASURE_SCOPE("Rendering frame");
    const Size level = 1 << max(int(fixed.subsampling) - int(iteration), 0);
    const Pixel size = params.camera->getSize();
    Pixel actSize;
    actSize.x = size.x / level + sgn(size.x % level);
    actSize.y = size.y / level + sgn(size.y % level);
    Bitmap<Rgba> bitmap(actSize);

    const bool first = (iteration == 0);
    parallelFor(*scheduler,
        threadData,
        0,
        Size(bitmap.size().y),
        1,
        [this, &bitmap, &params, level, first](Size y, ThreadData& data) {
            if (!shouldContinue && !first) {
                return;
            }
            for (Size x = 0; x < Size(bitmap.size().x); ++x) {
                const Coords pixel = Coords(x * level, y * level) +
                                     sampleTent2d(level, params.surface.filterWidth / 2.f, data.rng);
                const Optional<CameraRay> cameraRay = params.camera->unproject(pixel);
                if (!cameraRay) {
                    bitmap[Pixel(x, y)] = Rgba::black();
                    continue;
                }

                bitmap[Pixel(x, y)] = this->shade(params, cameraRay.value(), data);
            }
        });

    if (!shouldContinue && !first) {
        return;
    }
    if (level == 1) {
        fb.accumulate(*scheduler, bitmap);
    } else {
        Bitmap<Rgba> full(size);
        for (Size y = 0; y < Size(full.size().y); ++y) {
            for (Size x = 0; x < Size(full.size().x); ++x) {
                full[Pixel(x, y)] = bitmap[Pixel(x / level, y / level)];
            }
        }
        fb.override(std::move(full));
    }
}

void Raytracer::postProcess(FrameBuffer& fb,
    const RenderParams& params,
    const bool isFinal,
    IRenderOutput& output) const {
    if (!fixed.colorMap && (!isFinal || (!params.post.denoise && params.post.bloomIntensity == 0.f))) {
        // no postprocessing in this case, we can optimize and return the bitmap directly
        output.update(fb.getBitmap(), {}, isFinal);
        return;
    }

    Bitmap<Rgba> bitmap;
    if (isFinal) {
        bitmap = std::move(fb).getBitmap();
    } else {
        bitmap = fb.getBitmap().clone();
    }

    if (isFinal && params.post.bloomIntensity > 0.f) {
        bitmap = bloomEffect(*scheduler, bitmap, 30, params.post.bloomIntensity);
    }

    if (fixed.colorMap) {
        fixed.colorMap->map(*scheduler, bitmap);
    }

    if (isFinal && params.post.denoise) {
        bitmap = denoiseLowFrequency(*scheduler, bitmap, {});
    }

    output.update(std::move(bitmap), {}, isFinal);
}

Rgba Raytracer::getEnviroColor(const CameraRay& ray) const {
    if (fixed.enviro.hdri.empty()) {
        return fixed.enviro.color;
    } else {
        const Vector dir = ray.target - ray.origin;
        /// \todo deduplicate with setupUvws
        const SphericalCoords spherical = cartensianToSpherical(Vector(dir[X], dir[Z], dir[Y]));
        const Vector uvw(spherical.phi / (2._f * PI) + 0.5_f, spherical.theta / PI, 0._f);
        return fixed.enviro.hdri.eval(uvw);
    }
}

constexpr Size BLEND_ALL_FLAG = 0x80;
constexpr float MIN_NEIGHS = 8;

void Raytracer::initialize(const Storage& storage, const ICamera& UNUSED(camera)) {
    MEASURE_SCOPE("Building BVH");
    cached.r = storage.getValue<Vector>(QuantityId::POSITION).clone();

    if (storage.has(QuantityId::UVW)) {
        cached.uvws = storage.getValue<Vector>(QuantityId::UVW).clone();
    } else {
        cached.uvws.clear();
    }

    initializeFlags(storage);
    initializeVolumes(storage);
    initializeAttractors(storage);
    loadTextures(storage);
    evaluateShaders(storage);
    initializeStructures();

    for (ThreadData& data : threadData) {
        data.intersections.clear();
        data.neighs.clear();
        data.previousIdx = Size(-1);
    }

    shouldContinue = true;
}

void Raytracer::initializeFlags(const Storage& storage) {
    cached.flags.resize(storage.getParticleCnt());
    if (storage.has(QuantityId::FLAG) && storage.has(QuantityId::STRESS_REDUCING)) {
        ArrayView<const Size> idxs = storage.getValue<Size>(QuantityId::FLAG);
        ArrayView<const Float> reduce = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
        // avoid blending particles of different bodies, except if they are fully damaged
        for (Size i = 0; i < cached.flags.size(); ++i) {
            cached.flags[i] = idxs[i];
            if (reduce[i] == 0._f) {
                cached.flags[i] |= BLEND_ALL_FLAG;
            }
        }
    } else {
        cached.flags.fill(0);
    }
}

void Raytracer::initializeAttractors(const Storage& storage) {
    cached.attractors.resize(storage.getAttractorCnt());
    for (Size i = 0; i < storage.getAttractorCnt(); ++i) {
        const Attractor& a = storage.getAttractors()[i];
        cached.attractors[i].position = a.position;
        cached.attractors[i].radius = a.radius;

        // if (a.settings.has(AttractorSettingsId::VISUALIZATION_TEXTURE)) {
        //  String path = a.settings.get<String>(AttractorSettingsId::VISUALIZATION_TEXTURE);
        String path = "/home/pavel/projects/astro/sph/external/saturn.jpg";
        SharedPtr<Texture> texture = makeShared<Texture>(Path(path), TextureFiltering::BILINEAR);
        cached.attractors[i].texture = texture;
        //}
    }
}

void Raytracer::initializeVolumes(const Storage& storage) {
    cached.v.resize(storage.getParticleCnt());
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
        for (Size i = 0; i < cached.v.size(); ++i) {
            cached.v[i] = sphereVolume(cached.r[i][H]);
        }
    }
}

void Raytracer::loadTextures(const Storage& storage) {
    cached.materialIDs.resize(storage.getParticleCnt());
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
}

static void evaluateShader(RawPtr<IShader> shader, const Storage& storage, Array<Rgba>& data) {
    if (shader) {
        shader->initialize(storage, RefEnum::WEAK);
        data.resize(storage.getParticleCnt());
        for (Size i = 0; i < data.size(); ++i) {
            data[i] = shader->evaluateColor(i) * shader->evaluateScalar(i);
        }
    } else {
        data.clear();
    }
}

void Raytracer::evaluateShaders(const Storage& storage) {
    evaluateShader(shaders.emission.get(), storage, cached.emission);
    evaluateShader(shaders.scattering.get(), storage, cached.scattering);
    evaluateShader(shaders.absorption.get(), storage, cached.absorption);

    if (shaders.surfaceness) {
        shaders.surfaceness->initialize(storage, RefEnum::WEAK);
        cached.surfaceness.resize(storage.getParticleCnt());
        cached.albedo.resize(storage.getParticleCnt());
        for (Size i = 0; i < cached.albedo.size(); ++i) {
            cached.albedo[i] = shaders.surfaceness->evaluateColor(i);
            cached.surfaceness[i] = shaders.surfaceness->evaluateScalar(i);
        }
    } else {
        cached.surfaceness.clear();
        cached.albedo.clear();
    }
}

void Raytracer::initializeStructures() {
    finder = Factory::getFinder(RunSettings::getDefaults());
    finder->build(*scheduler, cached.r);

    Array<BvhSphere> spheres;
    spheres.resize(cached.r.size() + cached.attractors.size());
    cached.distention.resize(cached.r.size());

    ThreadLocal<Array<NeighborRecord>> neighs(*scheduler);
    parallelFor(*scheduler, neighs, 0, cached.r.size(), [&](const Size i, Array<NeighborRecord>& local) {
        const float initialRadius = cached.r[i][H];
        float radius = initialRadius;
        while (radius < fixed.maxDistention * initialRadius) {
            finder->findAll(cached.r[i], radius, local);
            if (local.size() >= MIN_NEIGHS) {
                break;
            } else {
                radius *= 1.5f;
            }
        }

        BvhSphere s(cached.r[i], radius);
        s.userData = i;
        spheres[i] = s;

        cached.distention[i] = min(radius / initialRadius, fixed.maxDistention);
    });

    for (Size i = 0; i < cached.attractors.size(); ++i) {
        BvhSphere s(cached.attractors[i].position, cached.attractors[i].radius);
        s.userData = cached.r.size() + i;
        spheres[i] = s;
    }

    bvh.build(std::move(spheres));
}

bool Raytracer::isInitialized() const {
    return !cached.r.empty();
}

Rgba Raytracer::shade(const RenderParams& params, const CameraRay& cameraRay, ThreadData& data) const {
    const Vector dir = getNormalized(cameraRay.target - cameraRay.origin);
    const Ray ray(cameraRay.origin, dir);

    // find all ray intersections
    data.intersections.clear();
    bvh.getAllIntersections(ray, backInserter(data.intersections));
    std::sort(data.intersections.begin(), data.intersections.end());

    // try to find surface
    Optional<Vector> hit = this->intersect(data, ray, params.surface.level);
    Rgba baseColor;
    float t_max;
    if (hit) {
        const Size index = data.previousIdx;
        // surface hit, starting color is the surface color
        if (this->isAttractor(index)) {
            baseColor = this->getAttractorColor(params, index - cached.r.size(), hit.value());
        } else {
            baseColor = this->getSurfaceColor(data, params, index, hit.value(), ray.direction());
        }
        t_max = getLength(ray.origin() - hit.value());
    } else {
        // no surface, use the environment color as the base
        baseColor = this->getEnviroColor(cameraRay);
        t_max = INFTY;
    }

    if (!cached.emission.empty() || !cached.absorption.empty() || !cached.scattering.empty()) {
        // modify color due to volume emission/absorption/scattering
        return this->getVolumeColor(data, params, cameraRay, baseColor, t_max);
    } else {
        return baseColor;
    }
}

ArrayView<const Size> Raytracer::getNeighborList(ThreadData& data, const Size index) const {
    // look for neighbors only if the intersected particle differs from the previous one
    if (index != data.previousIdx) {
        Array<NeighborRecord> neighs;
        finder->findAll(index, kernel.radius() * cached.r[index][H], neighs);
        data.previousIdx = index;

        // find the actual list of neighbors
        data.neighs.clear();
        for (NeighborRecord& n : neighs) {
            if (!this->canBeSurfaceHit(n.index)) {
                continue;
            }
            const Size flag1 = cached.flags[index];
            const Size flag2 = cached.flags[n.index];
            if ((flag1 & BLEND_ALL_FLAG) || (flag2 & BLEND_ALL_FLAG) || (flag1 == flag2)) {
                data.neighs.push(n.index);
            }
        }
    }
    return data.neighs;
}

Optional<Vector> Raytracer::intersect(ThreadData& data, const Ray& ray, const Float surfaceLevel) const {
    if (cached.attractors.empty() && cached.surfaceness.empty()) {
        return NOTHING;
    }

    for (const IntersectionInfo& intersect : data.intersections) {
        IntersectContext sc;
        sc.index = intersect.object->userData;
        sc.ray = ray;
        sc.t_min = intersect.t;
        sc.surfaceLevel = surfaceLevel;
        if (this->canBeSurfaceHit(sc.index)) {
            const Optional<Vector> hit = this->getSurfaceHit(data, sc, false);
            if (hit) {
                return hit;
            }
        }
        // rejected, process another intersection
    }
    return NOTHING;
}

bool Raytracer::occluded(ThreadData& data, const Ray& ray, const Float surfaceLevel) const {
    if (cached.attractors.empty() && cached.surfaceness.empty()) {
        return false;
    }

    bool occlusion = false;
    bvh.getIntersections(ray, [&](const IntersectionInfo& intersect) {
        IntersectContext sc;
        sc.index = intersect.object->userData;
        sc.ray = ray;
        sc.t_min = intersect.t;
        sc.surfaceLevel = surfaceLevel;
        if (this->canBeSurfaceHit(sc.index)) {
            const Optional<Vector> hit = this->getSurfaceHit(data, sc, true);
            if (hit) {
                occlusion = true;
                return false;
            }
        }
        // continue searching
        return true;
    });
    return occlusion;
}

bool Raytracer::canBeSurfaceHit(const Size index) const {
    if (this->isAttractor(index)) {
        // all attractors are fully opaque
        return true;
    } else {
        return !cached.surfaceness.empty() && cached.surfaceness[index] > 0.f;
    }
}

Optional<Vector> Raytracer::getSurfaceHit(ThreadData& data,
    const IntersectContext& context,
    bool occlusion) const {
    const Size i = context.index;
    const Ray& ray = context.ray;
    if (this->isAttractor(i)) {
        data.previousIdx = i;
        return ray.origin() + ray.direction() * context.t_min;
    }

    this->getNeighborList(data, i);

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
        phi = this->evalColorField(data.neighs, v2) - context.surfaceLevel;
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
        const float surfaceness = this->evalShader<float>(data.neighs, v2, cached.surfaceness);
        const float limit = data.rng();
        if (limit < surfaceness) {
            return v2;
        } else {
            return NOTHING;
        }
    }
}

Rgba Raytracer::getSurfaceColor(ThreadData& data,
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
    Rgba emission = Rgba::black();
    if (!cached.emission.empty()) {
        emission = this->evalShader<Rgba>(data.neighs, hit, cached.emission);
    }

    // compute the inward normal = gradient of the field
    const Vector n = this->evalNormal(data.neighs, hit);
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
        if (this->occluded(data, rayToSun, params.surface.level)) {
            // casted shadow
            return diffuse * params.surface.ambientLight + emission;
        }
    }

    // evaluate BRDF
    (void)dir;
    const Rgba f = cached.albedo[index]; /// \todo ?? // fixed.brdf->transport(n_norm, -dir, fixed.dirToSun);

    return diffuse * f * float(PI * cosPhi * params.surface.sunLight + params.surface.ambientLight) +
           emission;
}

Rgba Raytracer::getAttractorColor(const RenderParams& params, const Size index, const Vector& hit) const {
    const AttractorData& a = cached.attractors[index];
    Rgba diffuse = Rgba::white();
    if (a.texture) {
        const Vector r0 = hit - a.position;
        SphericalCoords spherical = cartensianToSpherical(r0);
        const Vector uvw = Vector(spherical.phi / (2._f * PI) + 0.5_f, spherical.theta / PI, 0._f);
        diffuse = a.texture->eval(uvw);
    }

    const Vector n = getNormalized(a.position - hit);
    const Float cosPhi = dot(n, fixed.dirToSun);
    if (cosPhi <= 0._f) {
        // not illuminated -> just ambient light
        return diffuse * params.surface.ambientLight;
    }

    return diffuse * float(PI * cosPhi * params.surface.sunLight + params.surface.ambientLight);
}

Rgba Raytracer::getVolumeColor(ThreadData& data,
    const RenderParams& params,
    const CameraRay& ray,
    const Rgba& baseColor,
    const float t_max) const {
    const Vector dir = getNormalized(ray.target - ray.origin);

    Rgba result = baseColor;
    for (const IntersectionInfo& is : reverse(data.intersections)) {
        if (is.t > t_max || this->isAttractor(is.object->userData)) {
            continue; // behind the surface
        }
        const BvhSphere* s = static_cast<const BvhSphere*>(is.object);
        const Size i = s->userData;
        const Vector hit = ray.origin + dir * is.t;
        const Vector center = cached.r[i];
        const Vector toCenter = getNormalized(center - hit);
        const float cosPhi = abs(dot(toCenter, dir));
        const float distention = cached.distention[i];
        // smoothing length should not have effect on the total emission
        // const float radiiFactor = cached.r[i][] / cached.r[i][H];
        const float secant = 2._f * getLength(center - hit) * cosPhi;
        if (!cached.absorption.empty()) {
            // make dilated particles absorb more
            result = result * exp(-cached.absorption[i] * secant * distention * pow<3>(cosPhi));
        }

        if (!cached.emission.empty()) {
            // 3th power of cosPhi to give more weight to the sphere center,
            // divide by distention^3; distention should not affect the total emission
            const float emissionMagnitude = pow<3>(cosPhi / distention) * secant;
            result += cached.emission[i] * emissionMagnitude;
            result.a() += emissionMagnitude;
        }

        if (!cached.scattering.empty()) {
            float scatteringMagnitude = secant; // dot(dir, -fixed.dirToSun) * secant;
            if (scatteringMagnitude > 0.f) {
                if (fixed.shadows) {
                    Ray rayToSun(hit, -fixed.dirToSun);
                    if (this->occluded(data, rayToSun, params.surface.level)) {
                        // casted shadow
                        scatteringMagnitude = 0.f;
                    }
                }
                result += cached.scattering[i] * scatteringMagnitude;
                result.a() += scatteringMagnitude;
            }
        }
    }
    result.a() = min(result.a(), 1.f);
    return result;
}

Float Raytracer::evalColorField(ArrayView<const Size> neighs, const Vector& pos1) const {
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

Vector Raytracer::evalNormal(ArrayView<const Size> neighs, const Vector& pos1) const {
    Vector value(0._f);
    for (Size index : neighs) {
        const Vector& pos2 = cached.r[index];
        const Vector grad = kernel.grad(pos1 - pos2, pos2[H]);
        value += cached.v[index] * grad;
    }
    return value;
}

template <typename T>
T Raytracer::evalShader(ArrayView<const Size> neighs, const Vector& pos1, ArrayView<const T> data) const {
    SPH_ASSERT(!neighs.empty());
    T value = T(0.f);
    float weightSum = 0.f;
    for (Size index : neighs) {
        const Vector& pos2 = cached.r[index];
        const float w = float(kernel.value(pos1 - pos2, pos2[H]) * cached.v[index]);
        value += data[index] * w;
        weightSum += w;
    }
    SPH_ASSERT(weightSum != 0._f);
    return value / weightSum;
}

constexpr Float SEAM_WIDTH = 0.1_f;

Vector Raytracer::evalUvws(ArrayView<const Size> neighs, const Vector& pos1) const {
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
