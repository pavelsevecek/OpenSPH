#include "gui/Factory.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Element.h"

NAMESPACE_SPH_BEGIN

AutoPtr<Abstract::Camera> Factory::getCamera(const GuiSettings& settings) {
    OrthoEnum id = settings.get<OrthoEnum>(GuiSettingsId::ORTHO_PROJECTION);
    OrthoCameraData data;
    data.fov = 240.f / settings.get<Float>(GuiSettingsId::VIEW_FOV);
    data.cutoff = settings.get<Float>(GuiSettingsId::ORTHO_CUTOFF);
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
    /// \todo generalize resolution
    const Point size(640, 480);
    const Vector center(settings.get<Vector>(GuiSettingsId::VIEW_CENTER));
    return makeAuto<OrthoCamera>(size, Point(int(center[X]), int(center[Y])), data);
}

AutoPtr<Abstract::Element> Factory::getElement(const GuiSettings& settings, const ElementId id) {
    Range range;
    switch (id) {
    case ElementId::VELOCITY:
        range = settings.get<Range>(GuiSettingsId::PALETTE_VELOCITY);
        return makeAuto<VelocityElement>(range);
    case ElementId::ACCELERATION:
        range = settings.get<Range>(GuiSettingsId::PALETTE_ACCELERATION);
        return makeAuto<AccelerationElement>(range);
    default:
        QuantityId quantity = QuantityId(id);
        ASSERT(int(quantity) >= 0);

        switch (quantity) {
        case QuantityId::NEIGHBOUR_CNT: // represent boundary element
            return makeAuto<BoundaryElement>(BoundaryElement::Detection::NEIGBOUR_THRESHOLD, 40);
        case QuantityId::DEVIATORIC_STRESS:
            range = settings.get<Range>(GuiSettingsId::PALETTE_STRESS);
            return makeAuto<TypedElement<TracelessTensor>>(quantity, range);
        case QuantityId::DENSITY:
            range = settings.get<Range>(GuiSettingsId::PALETTE_DENSITY);
            break;
        case QuantityId::PRESSURE:
            range = settings.get<Range>(GuiSettingsId::PALETTE_PRESSURE);
            break;
        case QuantityId::ENERGY:
            range = settings.get<Range>(GuiSettingsId::PALETTE_ENERGY);
            break;
        case QuantityId::DAMAGE:
            range = settings.get<Range>(GuiSettingsId::PALETTE_DAMAGE);
            break;
        case QuantityId::VELOCITY_DIVERGENCE:
            range = settings.get<Range>(GuiSettingsId::PALETTE_DIVV);
            break;
        default:
            NOT_IMPLEMENTED;
        }
        return makeAuto<TypedElement<Float>>(quantity, range);
    }
}

Palette Factory::getPalette(const ElementId id, const Range range) {
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
        case ElementId::VELOCITY:
            return Palette({ { x0, Color(0.5f, 0.5f, 0.5f) },
                               { x0 + 0.001f * dx, Color(0.0f, 0.0f, 0.2f) },
                               { x0 + 0.01f * dx, Color(0.0f, 0.0f, 1.0f) },
                               { x0 + 0.1f * dx, Color(1.0f, 0.0f, 0.2f) },
                               { x0 + dx, Color(1.0f, 1.0f, 0.2f) } },
                PaletteScale::LOGARITHMIC);
        case ElementId::ACCELERATION:
            return Palette({ { x0, Color(0.5f, 0.5f, 0.5f) },
                               { x0 + 0.001f * dx, Color(0.0f, 0.0f, 0.2f) },
                               { x0 + 0.01f * dx, Color(0.0f, 0.0f, 1.0f) },
                               { x0 + 0.1f * dx, Color(1.0f, 0.0f, 0.2f) },
                               { x0 + dx, Color(1.0f, 1.0f, 0.2f) } },
                PaletteScale::LOGARITHMIC);
        case ElementId::DIRECTION:
            ASSERT(range == Range(0._f, 2._f * PI)); // in radians
            return Palette({ { 0._f, Color(0.1_f, 0.1_f, 1._f) },
                               { PI, Color(1._f, 0.1_f, 0.1_f) },
                               { 2._f * PI, Color(0.1_f, 0.1_f, 1._f) } },
                PaletteScale::LINEAR);
        default:
            NOT_IMPLEMENTED;
        }
    }
}

NAMESPACE_SPH_END
