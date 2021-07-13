#include "gui/Factory.h"
#include "gui/Project.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/Brdf.h"
#include "gui/renderers/ContourRenderer.h"
#include "gui/renderers/FrameBuffer.h"
#include "gui/renderers/MeshRenderer.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/RayTracer.h"
#include "gui/renderers/VolumeRenderer.h"

NAMESPACE_SPH_BEGIN

AutoPtr<ITracker> Factory::getTracker(const GuiSettings& settings) {
    const int trackedIndex = settings.get<int>(GuiSettingsId::CAMERA_TRACK_PARTICLE);
    if (trackedIndex >= 0) {
        return makeAuto<ParticleTracker>(trackedIndex);
    }
    const bool useMedian = settings.get<bool>(GuiSettingsId::CAMERA_TRACK_MEDIAN);
    if (useMedian) {
        const Vector offset = settings.get<Vector>(GuiSettingsId::CAMERA_TRACKING_OFFSET);
        return makeAuto<MedianTracker>(offset);
    }
    return nullptr;
}

AutoPtr<ICamera> Factory::getCamera(const GuiSettings& settings, const Pixel size) {
    CameraEnum cameraId = settings.get<CameraEnum>(GuiSettingsId::CAMERA_TYPE);
    CameraParams data;
    data.imageSize = size;
    data.position = settings.get<Vector>(GuiSettingsId::CAMERA_POSITION);
    data.target = settings.get<Vector>(GuiSettingsId::CAMERA_TARGET);
    data.up = settings.get<Vector>(GuiSettingsId::CAMERA_UP);
    data.clipping = Interval(settings.get<Float>(GuiSettingsId::CAMERA_CLIP_NEAR),
        settings.get<Float>(GuiSettingsId::CAMERA_CLIP_FAR));
    data.perspective.fov = settings.get<Float>(GuiSettingsId::CAMERA_PERSPECTIVE_FOV);
    data.ortho.fov = float(settings.get<Float>(GuiSettingsId::CAMERA_ORTHO_FOV));
    data.ortho.cutoff = float(settings.get<Float>(GuiSettingsId::CAMERA_ORTHO_CUTOFF));
    if (data.ortho.cutoff.value() == 0._f) {
        data.ortho.cutoff = NOTHING;
    }

    switch (cameraId) {
    case CameraEnum::ORTHO:
        return makeAuto<OrthoCamera>(data);
    case CameraEnum::PERSPECTIVE:
        return makeAuto<PerspectiveCamera>(data);
    case CameraEnum::FISHEYE:
        return makeAuto<FisheyeCamera>(data);
    case CameraEnum::SPHERICAL:
        return makeAuto<SphericalCamera>(data);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IRenderer> Factory::getRenderer(const GuiSettings& settings) {
    SharedPtr<IScheduler> scheduler = getScheduler(RunSettings::getDefaults());
    return getRenderer(scheduler, settings);
}

AutoPtr<IRenderer> Factory::getRenderer(SharedPtr<IScheduler> scheduler, const GuiSettings& settings) {
    RendererEnum id = settings.get<RendererEnum>(GuiSettingsId::RENDERER);
    AutoPtr<IRenderer> renderer;
    switch (id) {
    case RendererEnum::NONE:
        class NullRenderer : public IRenderer {
            virtual void initialize(const Storage&, const IColorizer&, const ICamera&) override {}
            virtual void render(const RenderParams&, Statistics&, IRenderOutput&) const override {}
            virtual void cancelRender() override {}
        };
        renderer = makeAuto<NullRenderer>();
        break;
    case RendererEnum::PARTICLE:
        renderer = makeAuto<ParticleRenderer>(settings);
        break;
    case RendererEnum::MESH:
        renderer = makeAuto<MeshRenderer>(scheduler, settings);
        break;
    case RendererEnum::RAYMARCHER:
        renderer = makeAuto<RayMarcher>(scheduler, settings);
        break;
    case RendererEnum::VOLUME:
        renderer = makeAuto<VolumeRenderer>(scheduler, settings);
        break;
    case RendererEnum::CONTOUR:
        renderer = makeAuto<ContourRenderer>(scheduler, settings);
        break;
    default:
        NOT_IMPLEMENTED;
    }

    return renderer;
}

AutoPtr<IBrdf> Factory::getBrdf(const GuiSettings& settings) {
    const BrdfEnum id = settings.get<BrdfEnum>(GuiSettingsId::RAYTRACE_BRDF);
    switch (id) {
    case BrdfEnum::LAMBERT:
        return makeAuto<LambertBrdf>(1._f);
    case BrdfEnum::PHONG:
        return makeAuto<PhongBrdf>(1._f);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IColorMap> Factory::getColorMap(const GuiSettings& settings) {
    const ColorMapEnum id = settings.get<ColorMapEnum>(GuiSettingsId::COLORMAP_TYPE);
    switch (id) {
    case ColorMapEnum::LINEAR:
        return nullptr;
    case ColorMapEnum::LOGARITHMIC: {
        const float factor = settings.get<Float>(GuiSettingsId::COLORMAP_LOGARITHMIC_FACTOR);
        return makeAuto<LogarithmicColorMap>(factor);
    }
    case ColorMapEnum::FILMIC:
        return makeAuto<FilmicColorMap>();
    default:
        NOT_IMPLEMENTED;
    }
}

static AutoPtr<IColorizer> getColorizer(const GuiSettings& settings, const ExtColorizerId id) {
    using Factory::getPalette;

    switch (id) {
    case ColorizerId::VELOCITY:
        return makeAuto<VelocityColorizer>(getPalette(id));
    case ColorizerId::ACCELERATION:
        return makeAuto<AccelerationColorizer>(getPalette(id));
    case ColorizerId::MOVEMENT_DIRECTION:
        return makeAuto<DirectionColorizer>(Vector(0._f, 0._f, 1._f), getPalette(id));
    case ColorizerId::COROTATING_VELOCITY:
        return makeAuto<CorotatingVelocityColorizer>(getPalette(ColorizerId::VELOCITY));
    case ColorizerId::DENSITY_PERTURBATION:
        return makeAuto<DensityPerturbationColorizer>(getPalette(id));
    case ColorizerId::SUMMED_DENSITY:
        return makeAuto<SummedDensityColorizer>(RunSettings::getDefaults(), getPalette(QuantityId::DENSITY));
    case ColorizerId::TOTAL_ENERGY:
        return makeAuto<EnergyColorizer>(getPalette(id));
    case ColorizerId::TEMPERATURE:
        return makeAuto<TemperatureColorizer>();
    case ColorizerId::TOTAL_STRESS:
        return makeAuto<StressColorizer>(getPalette(id));
    case ColorizerId::YIELD_REDUCTION:
        return makeAuto<YieldReductionColorizer>(getPalette(id));
    case ColorizerId::DAMAGE_ACTIVATION:
        return makeAuto<DamageActivationColorizer>(getPalette(id));
    case ColorizerId::RADIUS:
        return makeAuto<RadiusColorizer>(getPalette(id));
    case ColorizerId::BOUNDARY:
        return makeAuto<BoundaryColorizer>(BoundaryColorizer::Detection::NEIGBOUR_THRESHOLD, 40);
    case ColorizerId::UVW:
        return makeAuto<UvwColorizer>();
    case ColorizerId::PARTICLE_ID:
        return makeAuto<ParticleIdColorizer>(settings);
    case ColorizerId::COMPONENT_ID:
        return makeAuto<ComponentIdColorizer>(
            settings, Post::ComponentFlag::OVERLAP | Post::ComponentFlag::SORT_BY_MASS);
    case ColorizerId::BOUND_COMPONENT_ID:
        return makeAuto<ComponentIdColorizer>(
            settings, Post::ComponentFlag::ESCAPE_VELOCITY | Post::ComponentFlag::SORT_BY_MASS);
    case ColorizerId::AGGREGATE_ID:
        return makeAuto<AggregateIdColorizer>(settings);
    case ColorizerId::FLAG:
        return makeAuto<IndexColorizer>(QuantityId::FLAG, settings);
    case ColorizerId::MATERIAL_ID:
        return makeAuto<MaterialColorizer>(settings);
    case ColorizerId::BEAUTY:
        return makeAuto<BeautyColorizer>();
    default:
        QuantityId quantity = QuantityId(id);
        SPH_ASSERT(int(quantity) >= 0);

        Palette palette = getPalette(id);
        switch (getMetadata(quantity).expectedType) {
        case ValueEnum::INDEX:
            return makeAuto<TypedColorizer<Size>>(quantity, std::move(palette));
        case ValueEnum::SCALAR:
            return makeAuto<TypedColorizer<Float>>(quantity, std::move(palette));
        case ValueEnum::VECTOR:
            return makeAuto<TypedColorizer<Vector>>(quantity, std::move(palette));
        case ValueEnum::TRACELESS_TENSOR:
            return makeAuto<TypedColorizer<TracelessTensor>>(quantity, std::move(palette));
        case ValueEnum::SYMMETRIC_TENSOR:
            return makeAuto<TypedColorizer<SymmetricTensor>>(quantity, std::move(palette));
        default:
            NOT_IMPLEMENTED;
        }
    }
}

AutoPtr<IColorizer> Factory::getColorizer(const Project& project, const ExtColorizerId id) {
    AutoPtr<IColorizer> colorizer = Sph::getColorizer(project.getGuiSettings(), id);
    Optional<Palette> palette = colorizer->getPalette();
    if (palette && project.getPalette(colorizer->name(), palette.value())) {
        colorizer->setPalette(palette.value());
    }
    return colorizer;
}

struct PaletteDesc {
    Interval range;
    PaletteScale scale;
};

static FlatMap<ExtColorizerId, PaletteDesc> paletteDescs = {
    { QuantityId::DENSITY, { Interval(2650._f, 2750._f), PaletteScale::LINEAR } },
    { QuantityId::MASS, { Interval(1.e5_f, 1.e10_f), PaletteScale::LOGARITHMIC } },
    { QuantityId::PRESSURE, { Interval(-1.e5_f, 1.e10_f), PaletteScale::HYBRID } },
    { QuantityId::ENERGY, { Interval(1._f, 1.e6_f), PaletteScale::LOGARITHMIC } },
    { QuantityId::DEVIATORIC_STRESS, { Interval(0._f, 1.e10_f), PaletteScale::LINEAR } },
    { QuantityId::DAMAGE, { Interval(0._f, 1._f), PaletteScale::LINEAR } },
    { QuantityId::VELOCITY_DIVERGENCE, { Interval(-0.1_f, 0.1_f), PaletteScale::LINEAR } },
    { QuantityId::VELOCITY_GRADIENT, { Interval(0._f, 1.e-3_f), PaletteScale::LINEAR } },
    { QuantityId::VELOCITY_LAPLACIAN, { Interval(0._f, 1.e-3_f), PaletteScale::LINEAR } },
    { QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE, { Interval(0._f, 1.e-3_f), PaletteScale::LINEAR } },
    { QuantityId::VELOCITY_ROTATION, { Interval(0._f, 4._f), PaletteScale::LINEAR } },
    { QuantityId::SOUND_SPEED, { Interval(0._f, 5.e3_f), PaletteScale::LINEAR } },
    { QuantityId::VIBRATIONAL_VELOCITY, { Interval(0._f, 5.e3_f), PaletteScale::LINEAR } },
    { QuantityId::BULK_DENSITY, { Interval(2650._f, 2750._f), PaletteScale::LINEAR } },
    { QuantityId::AV_ALPHA, { Interval(0.1_f, 1.5_f), PaletteScale::LINEAR } },
    { QuantityId::AV_BALSARA, { Interval(0._f, 1._f), PaletteScale::LINEAR } },
    { QuantityId::AV_STRESS, { Interval(0._f, 1.e8_f), PaletteScale::LINEAR } },
    { QuantityId::ANGULAR_FREQUENCY, { Interval(0._f, 1.e-3_f), PaletteScale::LINEAR } },
    { QuantityId::MOMENT_OF_INERTIA, { Interval(0._f, 1.e10_f), PaletteScale::LINEAR } },
    { QuantityId::PHASE_ANGLE, { Interval(0._f, 10._f), PaletteScale::LINEAR } },
    { QuantityId::STRAIN_RATE_CORRECTION_TENSOR, { Interval(0._f, 5._f), PaletteScale::LINEAR } },
    { QuantityId::EPS_MIN, { Interval(0._f, 1._f), PaletteScale::LINEAR } },
    { QuantityId::FRICTION, { Interval(0._f, 1._f), PaletteScale::LINEAR } },
    { QuantityId::DELTASPH_DENSITY_GRADIENT, { Interval(0._f, 1._f), PaletteScale::LINEAR } },
    { QuantityId::NEIGHBOUR_CNT, { Interval(50._f, 150._f), PaletteScale::LINEAR } },
    { ColorizerId::VELOCITY, { Interval(0.1_f, 100._f), PaletteScale::LOGARITHMIC } },
    { ColorizerId::ACCELERATION, { Interval(0.1_f, 100._f), PaletteScale::LOGARITHMIC } },
    { ColorizerId::MOVEMENT_DIRECTION, { Interval(0._f, 2._f * PI), PaletteScale::LINEAR } },
    { ColorizerId::RADIUS, { Interval(0._f, 1.e3_f), PaletteScale::LINEAR } },
    { ColorizerId::TOTAL_ENERGY, { Interval(1.e6_f, 1.e10_f), PaletteScale::LOGARITHMIC } },
    { ColorizerId::TEMPERATURE, { Interval(100._f, 1.e7_f), PaletteScale::LOGARITHMIC } },
    { ColorizerId::DENSITY_PERTURBATION, { Interval(-1.e-6_f, 1.e-6_f), PaletteScale::LINEAR } },
    { ColorizerId::DAMAGE_ACTIVATION, { Interval(2.e-4_f, 8.e-4_f), PaletteScale::LINEAR } },
    { ColorizerId::YIELD_REDUCTION, { Interval(0._f, 1._f), PaletteScale::LINEAR } },
    { ColorizerId::TOTAL_STRESS, { Interval(0._f, 1e6_f), PaletteScale::LINEAR } },
};

static Palette getDefaultPalette(const Interval range) {
    const float x0 = float(range.lower());
    const float dx = float(range.size());
    return Palette({ { x0, Rgba(0.f, 0.f, 0.6f) },
                       { x0 + 0.2f * dx, Rgba(0.1f, 0.1f, 0.1f) },
                       { x0 + 0.6f * dx, Rgba(0.9f, 0.9f, 0.9f) },
                       { x0 + 0.8f * dx, Rgba(1.f, 1.f, 0.f) },
                       { x0 + dx, Rgba(0.6f, 0.f, 0.f) } },
        PaletteScale::LINEAR);
}

Palette Factory::getPalette(const ExtColorizerId id) {
    const PaletteDesc desc = paletteDescs[id];
    const Interval range = desc.range;
    const PaletteScale scale = desc.scale;
    const float x0 = float(range.lower());
    const float dx = float(range.size());
    switch (id) {
    case ColorizerId::VELOCITY:
        return Palette({ { x0, Rgba(0.5f, 0.5f, 0.5f) },
                           { x0 + 0.001f * dx, Rgba(0.0f, 0.0f, 0.2f) },
                           { x0 + 0.01f * dx, Rgba(0.0f, 0.0f, 1.0f) },
                           { x0 + 0.1f * dx, Rgba(1.0f, 0.0f, 0.2f) },
                           { x0 + dx, Rgba(1.0f, 1.0f, 0.2f) } },
            scale);
    case ColorizerId::ACCELERATION:
        return Palette({ { x0, Rgba(0.5f, 0.5f, 0.5f) },
                           { x0 + 0.001f * dx, Rgba(0.0f, 0.0f, 0.2f) },
                           { x0 + 0.01f * dx, Rgba(0.0f, 0.0f, 1.0f) },
                           { x0 + 0.1f * dx, Rgba(1.0f, 0.0f, 0.2f) },
                           { x0 + dx, Rgba(1.0f, 1.0f, 0.2f) } },
            scale);
    case ColorizerId::MOVEMENT_DIRECTION: {
        SPH_ASSERT(range == Interval(0.f, 2._f * PI)); // in radians
        const float pi = float(PI);
        return Palette({ { 0.f, Rgba(0.1f, 0.1f, 1.f) },
                           { pi / 3.f, Rgba(1.f, 0.1f, 1.f) },
                           { 2.f * pi / 3.f, Rgba(1.f, 0.1f, 0.1f) },
                           { 3.f * pi / 3.f, Rgba(1.f, 1.f, 0.1f) },
                           { 4.f * pi / 3.f, Rgba(0.1f, 1.f, 0.1f) },
                           { 5.f * pi / 3.f, Rgba(0.1f, 1.f, 1.f) },
                           { 2.f * pi, Rgba(0.1f, 0.1f, 1.f) } },
            scale);
    }
    case ColorizerId::DENSITY_PERTURBATION:
        return Palette({ { x0, Rgba(0.1f, 0.1f, 1.f) },
                           { x0 + 0.5f * dx, Rgba(0.7f, 0.7f, 0.7f) },
                           { x0 + dx, Rgba(1.f, 0.1f, 0.1f) } },
            scale);
    case ColorizerId::TOTAL_ENERGY:
        return Palette({ { x0, Rgba(0.f, 0.f, 0.6f) },
                           { x0 + 0.01f * dx, Rgba(0.1f, 0.1f, 0.1f) },
                           { x0 + 0.05f * dx, Rgba(0.9f, 0.9f, 0.9f) },
                           { x0 + 0.2f * dx, Rgba(1.f, 1.f, 0.f) },
                           { x0 + dx, Rgba(0.6f, 0.f, 0.f) } },
            scale);
    case ColorizerId::TEMPERATURE:
        return Palette({ { x0, Rgba(0.1f, 0.1f, 0.1f) },
                           { x0 + 0.001f * dx, Rgba(0.1f, 0.1f, 1.f) },
                           { x0 + 0.01f * dx, Rgba(1.f, 0.f, 0.f) },
                           { x0 + 0.1f * dx, Rgba(1.0f, 0.6f, 0.4f) },
                           { x0 + dx, Rgba(1.f, 1.f, 0.f) } },
            scale);
    case ColorizerId::YIELD_REDUCTION:
        return Palette({ { 0._f, Rgba(0.1f, 0.1f, 0.1f) }, { 1._f, Rgba(0.9f, 0.9f, 0.9f) } }, scale);
    case ColorizerId::DAMAGE_ACTIVATION:
        return Palette({ { x0, Rgba(0.1f, 0.1f, 1.f) },
                           { x0 + 0.5f * dx, Rgba(0.7f, 0.7f, 0.7f) },
                           { x0 + dx, Rgba(1.f, 0.1f, 0.1f) } },
            scale);
    case ColorizerId::RADIUS:
        return Palette({ { x0, Rgba(0.1f, 0.1f, 1.f) },
                           { x0 + 0.5f * dx, Rgba(0.7f, 0.7f, 0.7f) },
                           { x0 + dx, Rgba(1.f, 0.1f, 0.1f) } },
            scale);
    default:
        // check extended values
        switch (QuantityId(id)) {
        case QuantityId::PRESSURE:
            SPH_ASSERT(x0 < -1.f);
            return Palette({ { x0, Rgba(0.3f, 0.3f, 0.8f) },
                               { -1.e4f, Rgba(0.f, 0.f, 0.2f) },
                               { 0.f, Rgba(0.2f, 0.2f, 0.2f) },
                               { 1.e4f, Rgba(0.8f, 0.8f, 0.8f) },
                               { 2.e4f, Rgba(1.f, 1.f, 0.2f) },
                               { x0 + dx, Rgba(0.5f, 0.f, 0.f) } },
                scale);
        case QuantityId::ENERGY:
            return Palette({ { x0, Rgba(0.1f, 0.1f, 0.1f) },
                               { x0 + 0.001f * dx, Rgba(0.1f, 0.1f, 1.f) },
                               { x0 + 0.01f * dx, Rgba(1.f, 0.f, 0.f) },
                               { x0 + 0.1f * dx, Rgba(1.0f, 0.6f, 0.4f) },
                               { x0 + dx, Rgba(1.f, 1.f, 0.f) } },
                scale);
        case QuantityId::DEVIATORIC_STRESS:
            return Palette({ { x0, Rgba(0.f, 0.f, 0.2f) },
                               { x0 + 0.1f * dx, Rgba(0.9f, 0.9f, 0.9f) },
                               { x0 + 0.25f * dx, Rgba(1.f, 1.f, 0.2f) },
                               { x0 + 0.5f * dx, Rgba(1.f, 0.5f, 0.f) },
                               { x0 + dx, Rgba(0.5f, 0.f, 0.f) } },
                scale);
        case QuantityId::DENSITY:
        case QuantityId::BULK_DENSITY:
        case QuantityId::VELOCITY_LAPLACIAN:
        case QuantityId::FRICTION:
        case QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE:
            return Palette({ { x0, Rgba(0.4f, 0.f, 0.4f) },
                               { x0 + 0.3f * dx, Rgba(0.3f, 0.3f, 1.f) },
                               { x0 + 0.5f * dx, Rgba(0.9f, 0.9f, 0.9f) },
                               { x0 + 0.7f * dx, Rgba(1.f, 0.f, 0.f) },
                               { x0 + dx, Rgba(1.f, 1.f, 0.f) } },
                scale);
        case QuantityId::DAMAGE:
            return Palette({ { x0, Rgba(0.1f, 0.1f, 0.1f) }, { x0 + dx, Rgba(0.9f, 0.9f, 0.9f) } }, scale);
        case QuantityId::MASS:
            return Palette({ { x0, Rgba(0.1f, 0.1f, 0.1f) }, { x0 + dx, Rgba(0.9f, 0.9f, 0.9f) } }, scale);
        case QuantityId::VELOCITY_DIVERGENCE:
            SPH_ASSERT(x0 < 0._f);
            return Palette({ { x0, Rgba(0.3f, 0.3f, 0.8f) },
                               { 0.1f * x0, Rgba(0.f, 0.f, 0.2f) },
                               { 0.f, Rgba(0.2f, 0.2f, 0.2f) },
                               { 0.1f * (x0 + dx), Rgba(0.8f, 0.8f, 0.8f) },
                               { x0 + dx, Rgba(1.0f, 0.6f, 0.f) } },
                scale);
        case QuantityId::VELOCITY_GRADIENT:
            SPH_ASSERT(x0 == 0._f);
            return Palette({ { 0._f, Rgba(0.3f, 0.3f, 0.8f) },
                               { 0.01f * dx, Rgba(0.f, 0.f, 0.2f) },
                               { 0.05f * dx, Rgba(0.2f, 0.2f, 0.2f) },
                               { 0.2f * dx, Rgba(0.8f, 0.8f, 0.8f) },
                               { dx, Rgba(1.0f, 0.6f, 0.f) } },
                scale);
        case QuantityId::ANGULAR_FREQUENCY:
            SPH_ASSERT(x0 == 0._f);
            return Palette({ { 0._f, Rgba(0.3f, 0.3f, 0.8f) },
                               { 0.25f * dx, Rgba(0.f, 0.f, 0.2f) },
                               { 0.5f * dx, Rgba(0.2f, 0.2f, 0.2f) },
                               { 0.75f * dx, Rgba(0.8f, 0.8f, 0.8f) },
                               { dx, Rgba(1.0f, 0.6f, 0.f) } },
                scale);
        case QuantityId::STRAIN_RATE_CORRECTION_TENSOR: {
            // sqrt(3) is an important value, as it corresponds to identity tensor
            const float actDx = max(dx, sqrt(3.f) + 0.2f);
            const float eps = 0.05f;
            return Palette({ { 0._f, Rgba(0.f, 0.0f, 0.5f) },
                               { sqrt(3.f) - eps, Rgba(0.9f, 0.9f, 0.9f) },
                               { sqrt(3.f), Rgba(1.f, 1.f, 0.f) },
                               { sqrt(3.f) + eps, Rgba(0.9f, 0.9f, 0.9f) },
                               { actDx, Rgba(0.5f, 0.0f, 0.0f) } },
                scale);
        }
        case QuantityId::AV_BALSARA:
            return Palette({ { x0, Rgba(0.1f, 0.1f, 0.1f) }, { x0 + dx, Rgba(0.9f, 0.9f, 0.9f) } }, scale);
        case QuantityId::EPS_MIN:
            return Palette({ { x0, Rgba(0.1f, 0.1f, 1.f) },
                               { x0 + 0.5f * dx, Rgba(0.7f, 0.7f, 0.7f) },
                               { x0 + dx, Rgba(1.f, 0.1f, 0.1f) } },
                scale);
        case QuantityId::MOMENT_OF_INERTIA:
            return Palette({ { x0, Rgba(0.1f, 0.1f, 1.f) },
                               { x0 + 0.5f * dx, Rgba(0.7f, 0.7f, 0.7f) },
                               { x0 + dx, Rgba(1.f, 0.1f, 0.1f) } },
                scale);
        default:
            return getDefaultPalette(Interval(x0, x0 + dx));
        }
    }
}

NAMESPACE_SPH_END
