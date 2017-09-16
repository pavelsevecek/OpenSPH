#include "gui/Factory.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/ParticleRenderer.h"
#include "gui/renderers/SurfaceRenderer.h"

NAMESPACE_SPH_BEGIN

AutoPtr<ICamera> Factory::getCamera(const GuiSettings& settings, const Point size) {
    OrthoEnum id = settings.get<OrthoEnum>(GuiSettingsId::ORTHO_PROJECTION);
    OrthoCameraData data;
    data.fov = 0.5_f * size.y / settings.get<Float>(GuiSettingsId::VIEW_FOV);
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
    const Vector center(settings.get<Vector>(GuiSettingsId::VIEW_CENTER));
    return makeAuto<OrthoCamera>(size, Point(int(center[X]), int(center[Y])), data);
}

AutoPtr<IRenderer> Factory::getRenderer(const GuiSettings& settings) {
    RendererEnum id = settings.get<RendererEnum>(GuiSettingsId::RENDERER);
    switch (id) {
    case RendererEnum::PARTICLE:
        return makeAuto<ParticleRenderer>(settings);
    case RendererEnum::SURFACE:
        return makeAuto<SurfaceRenderer>(settings);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<IColorizer> Factory::getColorizer(const GuiSettings& settings, const ColorizerId id) {
    Interval range;
    switch (id) {
    case ColorizerId::VELOCITY:
        range = settings.get<Interval>(GuiSettingsId::PALETTE_VELOCITY);
        return makeAuto<VelocityColorizer>(range);
    case ColorizerId::ACCELERATION:
        range = settings.get<Interval>(GuiSettingsId::PALETTE_ACCELERATION);
        return makeAuto<AccelerationColorizer>(range);
    case ColorizerId::MOVEMENT_DIRECTION:
        return makeAuto<DirectionColorizer>(Vector(0._f, 0._f, 1._f));
    case ColorizerId::COROTATING_VELOCITY:
        range = settings.get<Interval>(GuiSettingsId::PALETTE_VELOCITY);
        return makeAuto<CorotatingVelocityColorizer>(range);
    case ColorizerId::DENSITY_PERTURBATION:
        range = settings.get<Interval>(GuiSettingsId::PALETTE_DENSITY_PERTURB);
        return makeAuto<DensityPerturbationColorizer>(range);
    case ColorizerId::BOUNDARY:
        return makeAuto<BoundaryColorizer>(BoundaryColorizer::Detection::NEIGBOUR_THRESHOLD, 40);
    default:
        QuantityId quantity = QuantityId(id);
        ASSERT(int(quantity) >= 0);

        switch (quantity) {
        case QuantityId::DEVIATORIC_STRESS:
            range = settings.get<Interval>(GuiSettingsId::PALETTE_STRESS);
            return makeAuto<TypedColorizer<TracelessTensor>>(quantity, range);
        case QuantityId::DENSITY:
            range = settings.get<Interval>(GuiSettingsId::PALETTE_DENSITY);
            break;
        case QuantityId::PRESSURE:
            range = settings.get<Interval>(GuiSettingsId::PALETTE_PRESSURE);
            break;
        case QuantityId::ENERGY:
            range = settings.get<Interval>(GuiSettingsId::PALETTE_ENERGY);
            break;
        case QuantityId::DAMAGE:
            range = settings.get<Interval>(GuiSettingsId::PALETTE_DAMAGE);
            break;
        case QuantityId::VELOCITY_DIVERGENCE:
            range = settings.get<Interval>(GuiSettingsId::PALETTE_DIVV);
            break;
        default:
            NOT_IMPLEMENTED;
        }
        return makeAuto<TypedColorizer<Float>>(quantity, range);
    }
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
                               { -1.f, Color(0.f, 0.f, 0.2f) },
                               { 0.f, Color(0.2f, 0.2f, 0.2f) },
                               { 1.f, Color(0.8f, 0.8f, 0.8f) },
                               { 1.f + sqrt(dx), Color(1.f, 1.f, 0.2f) },
                               { x0 + dx, Color(0.5f, 0.f, 0.f) } },
                PaletteScale::HYBRID);
        case QuantityId::ENERGY:
            return Palette({ { x0, Color(0.7f, 0.7f, 0.7) },
                               { x0 + 0.001f * dx, Color(0.1f, 0.1f, 1.f) },
                               { x0 + 0.01f * dx, Color(1.f, 0.f, 0.f) },
                               { x0 + 0.1f * dx, Color(1.0f, 0.6f, 0.4f) },
                               { x0 + dx, Color(1.f, 1.f, 0.f) } },
                PaletteScale::LOGARITHMIC);
        case QuantityId::DEVIATORIC_STRESS:
            return Palette({ { x0, Color(0.f, 0.f, 0.2f) },
                               { x0 + sqrt(dx), Color(1.f, 1.f, 0.2f) },
                               { x0 + dx, Color(0.5f, 0.f, 0.f) } },
                PaletteScale::LOGARITHMIC);
        case QuantityId::DENSITY:
            return Palette({ { x0, Color(0.f, 0.f, 0.2f) },
                               { x0 + 0.5f * dx, Color(1.f, 1.f, 0.2f) },
                               { x0 + dx, Color(0.5f, 0.f, 0.f) } },
                PaletteScale::LINEAR);
        case QuantityId::DAMAGE:
            return Palette({ { x0, Color(0.1f, 0.1f, 0.1f) }, { x0 + dx, Color(0.9f, 0.9f, 0.9f) } },
                PaletteScale::LINEAR);
        case QuantityId::VELOCITY_DIVERGENCE:
            return Palette({ { x0, Color(0.3f, 0.3f, 0.8f) },
                               { -1.e-2f, Color(0.f, 0.f, 0.2f) },
                               { 0.f, Color(0.2f, 0.2f, 0.2f) },
                               { 1.e-2f, Color(0.8f, 0.8f, 0.8f) },
                               { x0 + dx, Color(1.0f, 0.6f, 0.f) } },
                PaletteScale::HYBRID);
        default:
            NOT_IMPLEMENTED;
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
        default:
            NOT_IMPLEMENTED;
        }
    }
}

NAMESPACE_SPH_END
