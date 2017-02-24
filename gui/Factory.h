#pragma once

#include "gui/Settings.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Element.h"

NAMESPACE_SPH_BEGIN

namespace Factory {
    static std::unique_ptr<Abstract::Camera> getCamera(const GuiSettings& settings) {
        OrthoEnum id = settings.get<OrthoEnum>(GuiSettingsIds::ORTHO_PROJECTION);
        OrthoCameraData data;
        data.fov = 240.f / settings.get<Float>(GuiSettingsIds::VIEW_FOV);
        data.cutoff = settings.get<Float>(GuiSettingsIds::ORTHO_CUTOFF);
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
        const Point size(640, 480);
        const Vector center(settings.get<Vector>(GuiSettingsIds::VIEW_CENTER));
        return std::make_unique<OrthoCamera>(size, Point(int(center[X]), int(center[Y])), data);
    }

    static std::unique_ptr<Abstract::Element> getElement(Storage& storage,
        const GuiSettings& settings,
        const QuantityIds id) {
        Range range;
        switch (id) {
        case QuantityIds::POSITIONS:
            range = settings.get<Range>(GuiSettingsIds::PALETTE_VELOCITY);
            return std::make_unique<VelocityElement>(storage, range);
        case QuantityIds::DEVIATORIC_STRESS:
            range = settings.get<Range>(GuiSettingsIds::PALETTE_STRESS);
            return std::make_unique<TypedElement<TracelessTensor>>(storage, id, range);
        case QuantityIds::DENSITY:
            range = settings.get<Range>(GuiSettingsIds::PALETTE_DENSITY);
            break;
        case QuantityIds::PRESSURE:
            range = settings.get<Range>(GuiSettingsIds::PALETTE_PRESSURE);
            break;
        case QuantityIds::ENERGY:
            range = settings.get<Range>(GuiSettingsIds::PALETTE_ENERGY);
            break;
        case QuantityIds::DAMAGE:
            range = settings.get<Range>(GuiSettingsIds::PALETTE_DAMAGE);
            break;
        default:
            NOT_IMPLEMENTED;
        }
        return std::make_unique<TypedElement<Float>>(storage, id, range);
    }
}

NAMESPACE_SPH_END
