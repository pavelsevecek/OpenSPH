#include "gui/Factory.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/Brdf.h"
#include "gui/renderers/MeshRenderer.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/RayTracer.h"

NAMESPACE_SPH_BEGIN

AutoPtr<ICamera> Factory::getCamera(const GuiSettings& settings, const Point size) {
    CameraEnum cameraId = settings.get<CameraEnum>(GuiSettingsId::CAMERA);
    switch (cameraId) {
    case CameraEnum::ORTHO: {
        OrthoEnum id = settings.get<OrthoEnum>(GuiSettingsId::ORTHO_PROJECTION);
        OrthoCameraData data;
        const float fov = settings.get<Float>(GuiSettingsId::ORTHO_FOV);
        if (fov != 0.f) {
            ASSERT(fov > 0.f);
            data.fov = 0.5_f * size.y / fov;
        } else {
            data.fov = NOTHING;
        }
        data.zoffset = settings.get<Float>(GuiSettingsId::ORTHO_ZOFFSET);
        switch (id) {
        case OrthoEnum::XY:
            data.u = Vector(1._f, 0._f, 0._f);
            data.v = Vector(0._f, 1._f, 0._f);
            break;
        case OrthoEnum::XZ:
            data.u = Vector(1._f, 0._f, 0._f);
            data.v = Vector(0._f, 0._f, 1._f);
            break;
        case OrthoEnum::YZ:
            data.u = Vector(0._f, 1._f, 0._f);
            data.v = Vector(0._f, 0._f, 1._f);
            break;
        default:
            NOT_IMPLEMENTED;
        }
        const Vector center(settings.get<Vector>(GuiSettingsId::ORTHO_VIEW_CENTER));
        return makeAuto<OrthoCamera>(size, Point(int(center[X]), int(center[Y])), data);
    }
    case CameraEnum::PERSPECTIVE: {
        PerspectiveCameraData data;
        data.position = settings.get<Vector>(GuiSettingsId::PERSPECTIVE_POSITION);
        data.target = settings.get<Vector>(GuiSettingsId::PERSPECTIVE_TARGET);
        data.up = settings.get<Vector>(GuiSettingsId::PERSPECTIVE_UP);
        data.fov = settings.get<Float>(GuiSettingsId::PERSPECTIVE_FOV);
        data.clipping = Interval(settings.get<Float>(GuiSettingsId::PERSPECTIVE_CLIP_NEAR),
            settings.get<Float>(GuiSettingsId::PERSPECTIVE_CLIP_FAR));
        const int trackedIndex = settings.get<int>(GuiSettingsId::PERSPECTIVE_TRACKED_PARTICLE);
        if (trackedIndex >= 0) {
            data.tracker = makeClone<ParticleTracker>(trackedIndex);
        }
        return makeAuto<PerspectiveCamera>(size, data);
    }
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IRenderer> Factory::getRenderer(IScheduler& scheduler, const GuiSettings& settings) {
    RendererEnum id = settings.get<RendererEnum>(GuiSettingsId::RENDERER);
    switch (id) {
    case RendererEnum::NONE:
        class NullRenderer : public IRenderer {
            virtual void initialize(const Storage&, const IColorizer&, const ICamera&) override {}
            virtual SharedPtr<wxBitmap> render(const ICamera&,
                const RenderParams&,
                Statistics&) const override {
                return nullptr;
            }
        };
        return makeAuto<NullRenderer>();
    case RendererEnum::PARTICLE:
        return makeAuto<ParticleRenderer>(settings);
    case RendererEnum::MESH:
        return makeAuto<MeshRenderer>(scheduler, settings);
    case RendererEnum::RAYTRACER:
        return makeAuto<RayTracer>(scheduler, settings);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IBrdf> Factory::getBrdf(const GuiSettings& UNUSED(settings)) {
    return makeAuto<LambertBrdf>(1._f);
}

static Palette getRealPalette(ColorizerId id,
    Variant<GuiSettingsId, Interval> rangeVariant,
    const GuiSettings& settings,
    const FlatMap<ColorizerId, Palette>& overrides) {
    Palette palette;
    if (auto overridenPalette = overrides.tryGet(id)) {
        return overridenPalette.value();
    } else {
        Interval range;
        if (Optional<GuiSettingsId> rangeId = rangeVariant.tryGet<GuiSettingsId>()) {
            range = settings.get<Interval>(rangeId.value());
        } else {
            range = rangeVariant.get<Interval>();
        }
        return Factory::getPalette(id, range);
    }
}

AutoPtr<IColorizer> Factory::getColorizer(const GuiSettings& settings,
    const ColorizerId id,
    const FlatMap<ColorizerId, Palette>& overrides) {
    switch (id) {
    case ColorizerId::VELOCITY:
        return makeAuto<VelocityColorizer>(
            getRealPalette(ColorizerId::VELOCITY, GuiSettingsId::PALETTE_VELOCITY, settings, overrides));
    case ColorizerId::ACCELERATION:
        return makeAuto<AccelerationColorizer>(getRealPalette(
            ColorizerId::ACCELERATION, GuiSettingsId::PALETTE_ACCELERATION, settings, overrides));
    case ColorizerId::MOVEMENT_DIRECTION:
        return makeAuto<DirectionColorizer>(Vector(0._f, 0._f, 1._f));
    case ColorizerId::COROTATING_VELOCITY:
        return makeAuto<CorotatingVelocityColorizer>(
            getRealPalette(ColorizerId::VELOCITY, GuiSettingsId::PALETTE_VELOCITY, settings, overrides));
    case ColorizerId::DENSITY_PERTURBATION:
        return makeAuto<DensityPerturbationColorizer>(getRealPalette(
            ColorizerId::DENSITY_PERTURBATION, GuiSettingsId::PALETTE_DENSITY_PERTURB, settings, overrides));
    case ColorizerId::SUMMED_DENSITY: {
        RunSettings dummy(EMPTY_SETTINGS);
        dummy.set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE);
        dummy.set(RunSettingsId::SPH_KERNEL, KernelEnum::CUBIC_SPLINE);
        return makeAuto<SummedDensityColorizer>(dummy,
            getRealPalette(
                ColorizerId(QuantityId::DENSITY), GuiSettingsId::PALETTE_DENSITY, settings, overrides));
    }
    case ColorizerId::TOTAL_ENERGY:
        return makeAuto<EnergyColorizer>(getRealPalette(
            ColorizerId::TOTAL_ENERGY, GuiSettingsId::PALETTE_TOTAL_ENERGY, settings, overrides));
    case ColorizerId::TOTAL_STRESS:
        return makeAuto<StressColorizer>(
            getRealPalette(ColorizerId::TOTAL_STRESS, GuiSettingsId::PALETTE_STRESS, settings, overrides));
    case ColorizerId::YIELD_REDUCTION:
        return makeAuto<YieldReductionColorizer>(
            getRealPalette(ColorizerId::YIELD_REDUCTION, Interval(0._f, 1._f), settings, overrides));
    case ColorizerId::DAMAGE_ACTIVATION:
        return makeAuto<DamageActivationColorizer>(
            getRealPalette(ColorizerId::DAMAGE_ACTIVATION, Interval(0._f, 1._f), settings, overrides));
    case ColorizerId::RADIUS:
        return makeAuto<RadiusColorizer>(
            getRealPalette(ColorizerId::RADIUS, GuiSettingsId::PALETTE_RADIUS, settings, overrides));
    case ColorizerId::BOUNDARY:
        return makeAuto<BoundaryColorizer>(BoundaryColorizer::Detection::NEIGBOUR_THRESHOLD, 40);
    case ColorizerId::UVW:
        return makeAuto<UvwColorizer>();
    case ColorizerId::PARTICLE_ID:
        return makeAuto<ParticleIdColorizer>();
    case ColorizerId::COMPONENT_ID:
        return makeAuto<ComponentIdColorizer>(Post::ComponentConnectivity::OVERLAP);
    case ColorizerId::BOUND_COMPONENT_ID:
        return makeAuto<ComponentIdColorizer>(Post::ComponentConnectivity::ESCAPE_VELOCITY);
    case ColorizerId::AGGREGATE_ID:
        return makeAuto<AggregateIdColorizer>();
    case ColorizerId::FLAG:
        return makeAuto<FlagColorizer>();
    case ColorizerId::BEAUTY:
        return makeAuto<BeautyColorizer>();
    default:
        QuantityId quantity = QuantityId(id);
        ASSERT(int(quantity) >= 0);
        Variant<GuiSettingsId, Interval> rangeVariant;

        switch (quantity) {
        case QuantityId::DEVIATORIC_STRESS: {
            rangeVariant = GuiSettingsId::PALETTE_STRESS;
            break;
        }
        case QuantityId::DENSITY:
            rangeVariant = GuiSettingsId::PALETTE_DENSITY;
            break;
        case QuantityId::MASS:
            rangeVariant = GuiSettingsId::PALETTE_MASS;
            break;
        case QuantityId::PRESSURE:
            rangeVariant = GuiSettingsId::PALETTE_PRESSURE;
            break;
        case QuantityId::ENERGY:
            rangeVariant = GuiSettingsId::PALETTE_ENERGY;
            break;
        case QuantityId::DAMAGE:
            rangeVariant = GuiSettingsId::PALETTE_DAMAGE;
            break;
        case QuantityId::VELOCITY_DIVERGENCE:
            rangeVariant = GuiSettingsId::PALETTE_DIVV;
            break;
        case QuantityId::VELOCITY_LAPLACIAN:
        case QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE:
            rangeVariant = GuiSettingsId::PALETTE_VELOCITY_SECOND_DERIVATIVES;
            break;
        case QuantityId::FRICTION:
            rangeVariant = Interval(0._f, 50._f);
            break;
        case QuantityId::AV_BALSARA:
            rangeVariant = Interval(0._f, 1._f);
            break;
        case QuantityId::EPS_MIN:
            rangeVariant = GuiSettingsId::PALETTE_ACTIVATION_STRAIN;
            break;
        case QuantityId::VELOCITY_GRADIENT:
            rangeVariant = GuiSettingsId::PALETTE_GRADV;
            break;
        case QuantityId::VELOCITY_ROTATION:
            rangeVariant = GuiSettingsId::PALETTE_ROTV;
            break;
        case QuantityId::ANGULAR_VELOCITY:
            rangeVariant = GuiSettingsId::PALETTE_ANGULAR_VELOCITY;
            break;
        case QuantityId::STRAIN_RATE_CORRECTION_TENSOR:
            rangeVariant = GuiSettingsId::PALETTE_STRAIN_RATE_CORRECTION_TENSOR;
            break;
        case QuantityId::MOMENT_OF_INERTIA:
            rangeVariant = GuiSettingsId::PALETTE_MOMENT_OF_INERTIA;
            break;
        default:
            NOT_IMPLEMENTED;
        }

        Palette palette = getRealPalette(id, rangeVariant, settings, overrides);
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

static Palette getDefaultPalette(const Interval range) {
    const float x0 = float(range.lower());
    const float dx = float(range.size());
    return Palette({ { x0, Color(0.f, 0.f, 0.6f) },
                       { x0 + 0.2f * dx, Color(0.1f, 0.1f, 0.1f) },
                       { x0 + 0.6f * dx, Color(0.9f, 0.9f, 0.9f) },
                       { x0 + 0.8f * dx, Color(1.f, 1.f, 0.f) },
                       { x0 + dx, Color(0.6f, 0.f, 0.f) } },
        PaletteScale::LINEAR);
}

Palette Factory::getPalette(const ColorizerId id, const Interval range) {
    const float x0 = Float(range.lower());
    const float dx = Float(range.size());
    if (int(id) >= 0) {
        QuantityId quantity = QuantityId(id);
        switch (quantity) {
        case QuantityId::PRESSURE:
            ASSERT(x0 < -1.f);
            return Palette({ { x0, Color(0.3f, 0.3f, 0.8f) },
                               { -1.e4f, Color(0.f, 0.f, 0.2f) },
                               { 0.f, Color(0.2f, 0.2f, 0.2f) },
                               { 1.e4f, Color(0.8f, 0.8f, 0.8f) },
                               { 2.e4f, Color(1.f, 1.f, 0.2f) },
                               { x0 + dx, Color(0.5f, 0.f, 0.f) } },
                PaletteScale::LINEAR);
        case QuantityId::ENERGY:
            return Palette({ { x0, Color(0.7f, 0.7f, 0.7) },
                               { x0 + 0.001f * dx, Color(0.1f, 0.1f, 1.f) },
                               { x0 + 0.01f * dx, Color(1.f, 0.f, 0.f) },
                               { x0 + 0.1f * dx, Color(1.0f, 0.6f, 0.4f) },
                               { x0 + dx, Color(1.f, 1.f, 0.f) } },
                PaletteScale::LOGARITHMIC);
            /*    return Palette({ { x0, Color(0.7f, 0.7f, 0.7) },
                                   { x0 + 0.01f * dx, Color(0.5f, 0.5f, 0.5f) },
                                   { x0 + 0.1f * dx, Color(0.5f, 0.5f, 0.5f) },
                                   { x0 + 0.3f * dx, Color(1.f, 0.f, 0.f) },
                                   { x0 + dx, Color(1.f, 1.f, 0.5) } },
                    PaletteScale::LOGARITHMIC);*/

        case QuantityId::DEVIATORIC_STRESS:
            return Palette({ { x0, Color(0.f, 0.f, 0.2f) },
                               { x0 + 0.1f * dx, Color(0.9f, 0.9f, 0.9f) },
                               { x0 + 0.25f * dx, Color(1.f, 1.f, 0.2f) },
                               { x0 + 0.5f * dx, Color(1.f, 0.5f, 0.f) },
                               { x0 + dx, Color(0.5f, 0.f, 0.f) } },
                PaletteScale::LOGARITHMIC);
        case QuantityId::DENSITY:
        case QuantityId::VELOCITY_LAPLACIAN:
        case QuantityId::FRICTION:
        case QuantityId::VELOCITY_GRADIENT_OF_DIVERGENCE:
            return Palette({ { x0, Color(0.1f, 0.1f, 0.1f) },
                               { x0 + 0.4f * dx, Color(0.2_f, 0.2_f, 1._f) },
                               { x0 + 0.5f * dx, Color(0.9f, 0.9f, 0.9f) },
                               { x0 + 0.6f * dx, Color(1.f, 0.f, 0.f) },
                               { x0 + dx, Color(1.f, 1.f, 0.f) } },
                PaletteScale::LINEAR);
        case QuantityId::DAMAGE:
            return Palette({ { x0, Color(0.1f, 0.1f, 0.1f) }, { x0 + dx, Color(0.9f, 0.9f, 0.9f) } },
                PaletteScale::LINEAR);
        case QuantityId::MASS:
            return Palette({ { x0, Color(0.1f, 0.1f, 0.1f) }, { x0 + dx, Color(0.9f, 0.9f, 0.9f) } },
                PaletteScale::LINEAR);
        case QuantityId::VELOCITY_DIVERGENCE:
            ASSERT(x0 < 0._f);
            return Palette({ { x0, Color(0.3f, 0.3f, 0.8f) },
                               { 0.1f * x0, Color(0.f, 0.f, 0.2f) },
                               { 0.f, Color(0.2f, 0.2f, 0.2f) },
                               { 0.1f * (x0 + dx), Color(0.8f, 0.8f, 0.8f) },
                               { x0 + dx, Color(1.0f, 0.6f, 0.f) } },
                PaletteScale::HYBRID);
        case QuantityId::VELOCITY_GRADIENT:
            ASSERT(x0 == 0._f);
            return Palette({ { 0._f, Color(0.3f, 0.3f, 0.8f) },
                               { 0.01f * dx, Color(0.f, 0.f, 0.2f) },
                               { 0.05f * dx, Color(0.2f, 0.2f, 0.2f) },
                               { 0.2f * dx, Color(0.8f, 0.8f, 0.8f) },
                               { dx, Color(1.0f, 0.6f, 0.f) } },
                PaletteScale::HYBRID);
        case QuantityId::ANGULAR_VELOCITY:
            ASSERT(x0 == 0._f);
            return Palette({ { 0._f, Color(0.3f, 0.3f, 0.8f) },
                               { 0.25f * dx, Color(0.f, 0.f, 0.2f) },
                               { 0.5f * dx, Color(0.2f, 0.2f, 0.2f) },
                               { 0.75f * dx, Color(0.8f, 0.8f, 0.8f) },
                               { dx, Color(1.0f, 0.6f, 0.f) } },
                PaletteScale::LINEAR);
        case QuantityId::STRAIN_RATE_CORRECTION_TENSOR:
            ASSERT(x0 > 0._f);
            return Palette({ { x0, Color(0.f, 0.f, 0.5f) },
                               { x0 + 0.01f, Color(0.1f, 0.1f, 0.1f) },
                               { x0 + 0.6f * dx, Color(0.9f, 0.9f, 0.9f) },
                               { x0 + dx, Color(0.6f, 0.0f, 0.0f) } },
                PaletteScale::LOGARITHMIC);
        case QuantityId::AV_BALSARA:
            return Palette({ { x0, Color(0.1f, 0.1f, 0.1f) }, { x0 + dx, Color(0.9f, 0.9f, 0.9f) } },
                PaletteScale::LINEAR);
        case QuantityId::EPS_MIN:
            return Palette({ { x0, Color(0.1f, 0.1f, 1.f) },
                               { x0 + 0.5f * dx, Color(0.7f, 0.7f, 0.7f) },
                               { x0 + dx, Color(1.f, 0.1f, 0.1f) } },
                PaletteScale::LINEAR);
        case QuantityId::MOMENT_OF_INERTIA:
            return Palette({ { x0, Color(0.1f, 0.1f, 1.f) },
                               { x0 + 0.5f * dx, Color(0.7f, 0.7f, 0.7f) },
                               { x0 + dx, Color(1.f, 0.1f, 0.1f) } },
                PaletteScale::LINEAR);
        default:
            return getDefaultPalette(Interval(x0, x0 + dx));
        }
    } else {
        switch (id) {
        case ColorizerId::VELOCITY:
            return Palette({ { x0, Color(0.5f, 0.5f, 0.5f) },
                               { x0 + 0.001f * dx, Color(0.0f, 0.0f, 0.2f) },
                               { x0 + 0.01f * dx, Color(0.0f, 0.0f, 1.0f) },
                               { x0 + 0.1f * dx, Color(1.0f, 0.0f, 0.2f) },
                               { x0 + dx, Color(1.0f, 1.0f, 0.2f) } },
                PaletteScale::LOGARITHMIC);
        case ColorizerId::ACCELERATION:
            return Palette({ { x0, Color(0.5f, 0.5f, 0.5f) },
                               { x0 + 0.001f * dx, Color(0.0f, 0.0f, 0.2f) },
                               { x0 + 0.01f * dx, Color(0.0f, 0.0f, 1.0f) },
                               { x0 + 0.1f * dx, Color(1.0f, 0.0f, 0.2f) },
                               { x0 + dx, Color(1.0f, 1.0f, 0.2f) } },
                PaletteScale::LOGARITHMIC);
        case ColorizerId::MOVEMENT_DIRECTION:
            ASSERT(range == Interval(0.f, 2.f * PI)); // in radians
            return Palette({ { 0.f, Color(0.1f, 0.1f, 1.f) },
                               { PI / 3.f, Color(1.f, 0.1f, 1.f) },
                               { 2.f * PI / 3.f, Color(1.f, 0.1f, 0.1f) },
                               { 3.f * PI / 3.f, Color(1.f, 1.f, 0.1f) },
                               { 4.f * PI / 3.f, Color(0.1f, 1.f, 0.1f) },
                               { 5.f * PI / 3.f, Color(0.1f, 1.f, 1.f) },
                               { 2.f * PI, Color(0.1f, 0.1f, 1.f) } },
                PaletteScale::LINEAR);
        case ColorizerId::DENSITY_PERTURBATION:
            return Palette({ { x0, Color(0.1f, 0.1f, 1.f) },
                               { x0 + 0.5f * dx, Color(0.7f, 0.7f, 0.7f) },
                               { x0 + dx, Color(1.f, 0.1f, 0.1f) } },
                PaletteScale::LINEAR);
        case ColorizerId::TOTAL_ENERGY:
            return Palette({ { x0, Color(0.f, 0.f, 0.6f) },
                               { x0 + 0.01f * dx, Color(0.1f, 0.1f, 0.1f) },
                               { x0 + 0.05f * dx, Color(0.9f, 0.9f, 0.9f) },
                               { x0 + 0.2f * dx, Color(1.f, 1.f, 0.f) },
                               { x0 + dx, Color(0.6f, 0.f, 0.f) } },
                PaletteScale::LOGARITHMIC);
        case ColorizerId::YIELD_REDUCTION:
            return Palette({ { 0._f, Color(0.1f, 0.1f, 0.1f) }, { 1._f, Color(0.9f, 0.9f, 0.9f) } },
                PaletteScale::LINEAR);
        case ColorizerId::DAMAGE_ACTIVATION:
            return Palette({ { x0, Color(0.1f, 0.1f, 1.f) },
                               { x0 + 0.5f * dx, Color(0.7f, 0.7f, 0.7f) },
                               { x0 + dx, Color(1.f, 0.1f, 0.1f) } },
                PaletteScale::LINEAR);
        case ColorizerId::RADIUS:
            return Palette({ { x0, Color(0.1f, 0.1f, 1.f) },
                               { x0 + 0.5f * dx, Color(0.7f, 0.7f, 0.7f) },
                               { x0 + dx, Color(1.f, 0.1f, 0.1f) } },
                PaletteScale::LINEAR);
        default:
            return getDefaultPalette(Interval(x0, x0 + dx));
        }
    }
}

NAMESPACE_SPH_END
