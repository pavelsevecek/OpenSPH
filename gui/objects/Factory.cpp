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

AutoPtr<Abstract::Element> Factory::getElement(const GuiSettings& settings, const QuantityId id) {
    Range range;
    switch (id) {
    case QuantityId::POSITIONS: // represents velocity element
        range = settings.get<Range>(GuiSettingsId::PALETTE_VELOCITY);
        return makeAuto<VelocityElement>(range);
    case QuantityId::NEIGHBOUR_CNT: // represent boundary element
        return makeAuto<BoundaryElement>(BoundaryElement::Detection::NORMAL_BASED, 10);
    case QuantityId::DEVIATORIC_STRESS:
        range = settings.get<Range>(GuiSettingsId::PALETTE_STRESS);
        return makeAuto<TypedElement<TracelessTensor>>(id, range);
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
    return makeAuto<TypedElement<Float>>(id, range);
}

NAMESPACE_SPH_END
