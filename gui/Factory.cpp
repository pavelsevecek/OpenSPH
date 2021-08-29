#include "gui/Factory.h"
#include "gui/Project.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/Brdf.h"
#include "gui/renderers/FrameBuffer.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/RayMarcher.h"

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
            virtual void initialize(const Storage&, const ICamera&) override {}
            virtual bool isInitialized() const override {
                return true;
            }
            virtual void render(const RenderParams&, Statistics&, IRenderOutput&) const override {}
            virtual void cancelRender() override {}
        };
        renderer = makeAuto<NullRenderer>();
        break;
    case RendererEnum::PARTICLE: {
        AutoPtr<IColorizer> colorizer = Factory::getColorizer(Project::getInstance(), ColorizerId::VELOCITY);
        renderer = makeAuto<ParticleRenderer>(settings, std::move(colorizer));
        break;
    }
    case RendererEnum::RAYTRACER:
        renderer = makeAuto<Raytracer>(scheduler, settings);
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
    using Factory::getColorLut;

    switch (id) {
    case ColorizerId::VELOCITY:
        return makeAuto<VelocityColorizer>(getColorLut(id));
    case ColorizerId::ACCELERATION:
        return makeAuto<AccelerationColorizer>(getColorLut(id));
    case ColorizerId::MOVEMENT_DIRECTION:
        return makeAuto<DirectionColorizer>(Vector(0._f, 0._f, 1._f), getColorLut(id));
    case ColorizerId::COROTATING_VELOCITY:
        return makeAuto<CorotatingVelocityColorizer>(getColorLut(ColorizerId::VELOCITY));
    case ColorizerId::DENSITY_PERTURBATION:
        return makeAuto<DensityPerturbationColorizer>(getColorLut(id));
    case ColorizerId::SUMMED_DENSITY:
        return makeAuto<SummedDensityColorizer>(RunSettings::getDefaults(), getColorLut(QuantityId::DENSITY));
    case ColorizerId::TOTAL_ENERGY:
        return makeAuto<EnergyColorizer>(getColorLut(id));
    case ColorizerId::TEMPERATURE:
        return makeAuto<TemperatureColorizer>();
    case ColorizerId::TOTAL_STRESS:
        return makeAuto<StressColorizer>(getColorLut(id));
    case ColorizerId::YIELD_REDUCTION:
        return makeAuto<YieldReductionColorizer>(getColorLut(id));
    case ColorizerId::DAMAGE_ACTIVATION:
        return makeAuto<DamageActivationColorizer>(getColorLut(id));
    case ColorizerId::RADIUS:
        return makeAuto<RadiusColorizer>(getColorLut(id));
    case ColorizerId::BOUNDARY:
        return makeAuto<BoundaryColorizer>(BoundaryColorizer::Detection::NEIGBOR_THRESHOLD, 40);
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
    default:
        QuantityId quantity = QuantityId(id);
        SPH_ASSERT(int(quantity) >= 0);

        ColorLut lut = getColorLut(id);
        switch (getMetadata(quantity).expectedType) {
        case ValueEnum::INDEX:
            return makeAuto<TypedColorizer<Size>>(quantity, std::move(lut));
        case ValueEnum::SCALAR:
            return makeAuto<TypedColorizer<Float>>(quantity, std::move(lut));
        case ValueEnum::VECTOR:
            return makeAuto<TypedColorizer<Vector>>(quantity, std::move(lut));
        case ValueEnum::TRACELESS_TENSOR:
            return makeAuto<TypedColorizer<TracelessTensor>>(quantity, std::move(lut));
        case ValueEnum::SYMMETRIC_TENSOR:
            return makeAuto<TypedColorizer<SymmetricTensor>>(quantity, std::move(lut));
        default:
            NOT_IMPLEMENTED;
        }
    }
}

AutoPtr<IColorizer> Factory::getColorizer(const Project& project, const ExtColorizerId id) {
    AutoPtr<IColorizer> colorizer = Sph::getColorizer(project.getGuiSettings(), id);
    Optional<ColorLut> lut = colorizer->getColorLut();
    if (lut && project.getColorLut(colorizer->name(), lut.value())) {
        colorizer->setColorLut(lut.value());
    }
    return colorizer;
}

struct ColorLutDesc {
    Interval range;
    PaletteScale scale;
};

static FlatMap<ExtColorizerId, ColorLutDesc> lutDescs(ELEMENTS_UNIQUE,
    {
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
        { QuantityId::AV_ALPHA, { Interval(0.1_f, 1.5_f), PaletteScale::LINEAR } },
        { QuantityId::AV_BALSARA, { Interval(0._f, 1._f), PaletteScale::LINEAR } },
        { QuantityId::AV_STRESS, { Interval(0._f, 1.e8_f), PaletteScale::LINEAR } },
        { QuantityId::ANGULAR_FREQUENCY, { Interval(0._f, 1.e-3_f), PaletteScale::LINEAR } },
        { QuantityId::MOMENT_OF_INERTIA, { Interval(0._f, 1.e10_f), PaletteScale::LINEAR } },
        { QuantityId::PHASE_ANGLE, { Interval(0._f, 10._f), PaletteScale::LINEAR } },
        { QuantityId::STRAIN_RATE_CORRECTION_TENSOR,
            { Interval(0._f, 2._f * sqrt(3._f)), PaletteScale::LINEAR } },
        { QuantityId::EPS_MIN, { Interval(0._f, 1._f), PaletteScale::LINEAR } },
        { QuantityId::FRICTION, { Interval(0._f, 1._f), PaletteScale::LINEAR } },
        { QuantityId::DELTASPH_DENSITY_GRADIENT, { Interval(0._f, 1._f), PaletteScale::LINEAR } },
        { QuantityId::NEIGHBOR_CNT, { Interval(50._f, 150._f), PaletteScale::LINEAR } },
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
    });

static Palette getDefaultPalette() {
    Palette palette({
        { 0.f, Rgba(0.f, 0.f, 0.6f) },
        { 0.2f, Rgba(0.1f, 0.1f, 0.1f) },
        { 0.6f, Rgba(0.9f, 0.9f, 0.9f) },
        { 0.8f, Rgba(1.f, 1.f, 0.f) },
        { 1.f, Rgba(0.6f, 0.f, 0.f) },
    });
    return palette;
}

Palette Factory::getPalette(const ExtColorizerId id) {
    switch (id) {
    case ColorizerId::VELOCITY:
        return Palette({
            { 0.f, Rgba(0.5f, 0.5f, 0.5f) },
            { 0.25f, Rgba(0.0f, 0.0f, 0.2f) },
            { 0.5f, Rgba(0.0f, 0.0f, 1.0f) },
            { 0.75f, Rgba(1.0f, 0.0f, 0.2f) },
            { 1.f, Rgba(1.0f, 1.0f, 0.2f) },
        });
    case ColorizerId::ACCELERATION:
        return Palette({
            { 0.f, Rgba(0.5f, 0.5f, 0.5f) },
            { 0.25f, Rgba(0.0f, 0.0f, 0.2f) },
            { 0.5f, Rgba(0.0f, 0.0f, 1.0f) },
            { 0.75f, Rgba(1.0f, 0.0f, 0.2f) },
            { 1.f, Rgba(1.0f, 1.0f, 0.2f) },
        });
    case ColorizerId::MOVEMENT_DIRECTION: {
        return Palette({
            { 0.f, Rgba(0.1f, 0.1f, 1.f) },
            { 1.f / 6.f, Rgba(1.f, 0.1f, 1.f) },
            { 2.f / 6.f, Rgba(1.f, 0.1f, 0.1f) },
            { 3.f / 6.f, Rgba(1.f, 1.f, 0.1f) },
            { 4.f / 6.f, Rgba(0.1f, 1.f, 0.1f) },
            { 5.f / 6.f, Rgba(0.1f, 1.f, 1.f) },
            { 1.f, Rgba(0.1f, 0.1f, 1.f) },
        });
    }
    case ColorizerId::DENSITY_PERTURBATION:
        return Palette({
            { 0.f, Rgba(0.1f, 0.1f, 1.f) },
            { 0.5f, Rgba(0.7f, 0.7f, 0.7f) },
            { 1.f, Rgba(1.f, 0.1f, 0.1f) },
        });
    case ColorizerId::TOTAL_ENERGY:
        return Palette({
            { 0.f, Rgba(0.f, 0.f, 0.6f) },
            { 0.2f, Rgba(0.1f, 0.1f, 0.1f) },
            { 0.5f, Rgba(0.9f, 0.9f, 0.9f) },
            { 0.75f, Rgba(1.f, 1.f, 0.f) },
            { 1.f, Rgba(0.6f, 0.f, 0.f) },
        });
    case ColorizerId::TEMPERATURE:
        return Palette({
            { 0.f, Rgba(0.1f, 0.1f, 0.1f) },
            { 0.25f, Rgba(0.1f, 0.1f, 1.f) },
            { 0.5f, Rgba(1.f, 0.f, 0.f) },
            { 0.75f, Rgba(1.0f, 0.6f, 0.4f) },
            { 1.f, Rgba(1.f, 1.f, 0.f) },
        });
    case ColorizerId::YIELD_REDUCTION:
        return Palette({
            { 0.f, Rgba(0.1f, 0.1f, 0.1f) },
            { 1.f, Rgba(0.9f, 0.9f, 0.9f) },
        });
    case ColorizerId::DAMAGE_ACTIVATION:
        return Palette({
            { 0.f, Rgba(0.1f, 0.1f, 1.f) },
            { 0.5f, Rgba(0.7f, 0.7f, 0.7f) },
            { 1.f, Rgba(1.f, 0.1f, 0.1f) },
        });
    case ColorizerId::RADIUS:
        return Palette({
            { 0.f, Rgba(0.1f, 0.1f, 1.f) },
            { 0.5f, Rgba(0.7f, 0.7f, 0.7f) },
            { 1.f, Rgba(1.f, 0.1f, 0.1f) },
        });
    default:
        // check extended values
        switch (QuantityId(id)) {
        case QuantityId::PRESSURE:
            return Palette({
                { 0.f, Rgba(0.3f, 0.3f, 0.8f) },
                { 0.2f, Rgba(0.f, 0.f, 0.2f) },
                { 0.4f, Rgba(0.2f, 0.2f, 0.2f) },
                { 0.6f, Rgba(0.8f, 0.8f, 0.8f) },
                { 0.8f, Rgba(1.f, 1.f, 0.2f) },
                { 1.f, Rgba(0.5f, 0.f, 0.f) },
            });
        case QuantityId::ENERGY:
            return Palette({
                { 0.f, Rgba(0.1f, 0.1f, 0.1f) },
                { 0.25f, Rgba(0.1f, 0.1f, 1.f) },
                { 0.5f, Rgba(1.f, 0.f, 0.f) },
                { 0.75, Rgba(1.0f, 0.6f, 0.4f) },
                { 1.f, Rgba(1.f, 1.f, 0.f) },
            });
        case QuantityId::DEVIATORIC_STRESS:
            return Palette({
                { 0.f, Rgba(0.f, 0.f, 0.2f) },
                { 0.25f, Rgba(0.9f, 0.9f, 0.9f) },
                { 0.5f, Rgba(1.f, 1.f, 0.2f) },
                { 0.75f, Rgba(1.f, 0.5f, 0.f) },
                { 1.f, Rgba(0.5f, 0.f, 0.f) },
            });
        case QuantityId::DENSITY:
        case QuantityId::VELOCITY_LAPLACIAN:
        case QuantityId::FRICTION:
        case QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE:
            return Palette({
                { 0._f, Rgba(0.4f, 0.f, 0.4f) },
                { 0.3f, Rgba(0.3f, 0.3f, 1.f) },
                { 0.5f, Rgba(0.9f, 0.9f, 0.9f) },
                { 0.7f, Rgba(1.f, 0.f, 0.f) },
                { 1.f, Rgba(1.f, 1.f, 0.f) },
            });
        case QuantityId::DAMAGE:
        case QuantityId::MASS:
            return Palette({
                { 0.f, Rgba(0.1f, 0.1f, 0.1f) },
                { 1.f, Rgba(0.9f, 0.9f, 0.9f) },
            });
        case QuantityId::VELOCITY_DIVERGENCE:
            return Palette({
                { 0.f, Rgba(0.3f, 0.3f, 0.8f) },
                { 0.2f, Rgba(0.f, 0.f, 0.2f) },
                { 0.5f, Rgba(0.2f, 0.2f, 0.2f) },
                { 0.7f, Rgba(0.8f, 0.8f, 0.8f) },
                { 1.f, Rgba(1.0f, 0.6f, 0.f) },
            });
        case QuantityId::VELOCITY_GRADIENT:
            return Palette({
                { 0._f, Rgba(0.3f, 0.3f, 0.8f) },
                { 0.25f, Rgba(0.f, 0.f, 0.2f) },
                { 0.5f, Rgba(0.2f, 0.2f, 0.2f) },
                { 0.75f, Rgba(0.8f, 0.8f, 0.8f) },
                { 1.f, Rgba(1.0f, 0.6f, 0.f) },
            });
        case QuantityId::ANGULAR_FREQUENCY:
            return Palette({
                { 0._f, Rgba(0.3f, 0.3f, 0.8f) },
                { 0.25f, Rgba(0.f, 0.f, 0.2f) },
                { 0.5f, Rgba(0.2f, 0.2f, 0.2f) },
                { 0.75f, Rgba(0.8f, 0.8f, 0.8f) },
                { 1.f, Rgba(1.0f, 0.6f, 0.f) },
            });
        case QuantityId::STRAIN_RATE_CORRECTION_TENSOR: {
            return Palette({
                { 0._f, Rgba(0.f, 0.0f, 0.5f) },
                { 0.4_f, Rgba(0.9f, 0.9f, 0.9f) },
                { 0.5_f, Rgba(1.f, 1.f, 0.f) },
                { 0.6_f, Rgba(0.9f, 0.9f, 0.9f) },
                { 1._f, Rgba(0.5f, 0.0f, 0.0f) },
            });
        }
        case QuantityId::AV_BALSARA:
            return Palette({
                { 0.f, Rgba(0.1f, 0.1f, 0.1f) },
                { 1.f, Rgba(0.9f, 0.9f, 0.9f) },
            });
        case QuantityId::EPS_MIN:
            return Palette({
                { 0.f, Rgba(0.1f, 0.1f, 1.f) },
                { 0.5f, Rgba(0.7f, 0.7f, 0.7f) },
                { 1.f, Rgba(1.f, 0.1f, 0.1f) },
            });
        case QuantityId::MOMENT_OF_INERTIA:
            return Palette({
                { 0.f, Rgba(0.1f, 0.1f, 1.f) },
                { 0.5f, Rgba(0.7f, 0.7f, 0.7f) },
                { 1.f, Rgba(1.f, 0.1f, 0.1f) },
            });
        default:
            return getDefaultPalette();
        }
    }
}

ColorLut Factory::getColorLut(const ExtColorizerId id) {
    const ColorLutDesc desc = lutDescs[id];
    const Interval range = desc.range;
    const PaletteScale scale = desc.scale;
    return ColorLut(getPalette(id), range, scale);
}

NAMESPACE_SPH_END
