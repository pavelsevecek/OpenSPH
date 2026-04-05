#include "gui/windows/OrthoPane.h"
#include "gui/Controller.h"
#include "gui/MainLoop.h"
#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "quantities/QuantityIds.h"
#include "quantities/Storage.h"
#include "thread/CheckFunction.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <vector>
#include <wx/dcbuffer.h>
#include <wx/image.h>

NAMESPACE_SPH_BEGIN

namespace {

String formatRulerDistance(const Float distance, const Float unitScale, const String& unitLabel) {
    const Float value = distance / unitScale;
    std::stringstream ss;
    if (abs(value) < 0.01_f || abs(value) > 1.e5_f) {
        ss << std::setprecision(3) << std::scientific << value;
    } else {
        ss << std::setprecision(6) << std::fixed << value;
        String raw = String::fromAscii(ss.str().c_str());
        while (!raw.empty() && raw[raw.size() - 1] == L'0') {
            raw = raw.substr(0, raw.size() - 1);
        }
        if (!raw.empty() && raw[raw.size() - 1] == L'.') {
            raw = raw.substr(0, raw.size() - 1);
        }
        return "Distance: " + raw + " " + unitLabel;
    }
    return "Distance: " + String::fromAscii(ss.str().c_str()) + " " + unitLabel;
}

void markPixel(std::vector<uint8_t>& mask, const int width, const int height, const int x, const int y) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    mask[Size(y * width + x)] = 1;
}

void markDisc(std::vector<uint8_t>& mask, const int width, const int height, const Coords& center, const int radius) {
    const int minX = int(std::floor(center.x)) - radius;
    const int maxX = int(std::ceil(center.x)) + radius;
    const int minY = int(std::floor(center.y)) - radius;
    const int maxY = int(std::ceil(center.y)) + radius;
    const Float radius2 = sqr(Float(radius) + 0.5_f);
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const Float dx = center.x - Float(x);
            const Float dy = center.y - Float(y);
            if (dx * dx + dy * dy <= radius2) {
                markPixel(mask, width, height, x, y);
            }
        }
    }
}

void markSegment(
    std::vector<uint8_t>& mask,
    const int width,
    const int height,
    const Coords& start,
    const Coords& end,
    const int radius
) {
    const Coords delta = end - start;
    const int steps = max(1, int(std::ceil(getLength(delta) * 2.f)));
    for (int i = 0; i <= steps; ++i) {
        const Float t = Float(i) / Float(steps);
        markDisc(mask, width, height, start + delta * t, radius);
    }
}

void markCap(
    std::vector<uint8_t>& mask,
    const int width,
    const int height,
    const Coords& center,
    const Coords& direction,
    const int halfLength,
    const int thickness
) {
    Coords normal(-direction.y, direction.x);
    const float length = max(getLength(normal), 1.f);
    normal /= length;
    const Coords delta = normal * Float(halfLength);
    markSegment(mask, width, height, center - delta, center + delta, thickness);
}

void applyContrastOverlay(wxImage& image, const std::vector<uint8_t>& mask) {
    if (!image.IsOk() || !image.GetData()) {
        return;
    }

    const int width = image.GetWidth();
    const int height = image.GetHeight();
    unsigned char* data = image.GetData();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const Size pixelIdx = Size(y * width + x);
            if (!mask[pixelIdx]) {
                continue;
            }
            const Size dataIdx = 3 * pixelIdx;
            const int luminance = (30 * int(data[dataIdx + 0]) + 59 * int(data[dataIdx + 1]) +
                11 * int(data[dataIdx + 2])) /
                100;
            const unsigned char overlay = luminance > 96 ? 0 : 255;
            data[dataIdx + 0] = overlay;
            data[dataIdx + 1] = overlay;
            data[dataIdx + 2] = overlay;
        }
    }
}

void applyColorOverlay(
    wxImage& image,
    const std::vector<uint8_t>& mask,
    const unsigned char red,
    const unsigned char green,
    const unsigned char blue
) {
    if (!image.IsOk() || !image.GetData()) {
        return;
    }

    const int width = image.GetWidth();
    const int height = image.GetHeight();
    unsigned char* data = image.GetData();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const Size pixelIdx = Size(y * width + x);
            if (!mask[pixelIdx]) {
                continue;
            }
            const Size dataIdx = 3 * pixelIdx;
            data[dataIdx + 0] = red;
            data[dataIdx + 1] = green;
            data[dataIdx + 2] = blue;
        }
    }
}

} // namespace

OrthoPane::OrthoPane(wxWindow* parent, Controller* controller, const GuiSettings& UNUSED(gui))
    : IGraphicsPane(parent)
    , controller(controller) {
    this->SetBackgroundStyle(wxBG_STYLE_PAINT);

    this->SetMinSize(wxSize(300, 300));
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(OrthoPane::onPaint));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(OrthoPane::onMouseMotion));
    this->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(OrthoPane::onMouseWheel));
    this->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(OrthoPane::onRightDown));
    this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(OrthoPane::onRightUp));
    this->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(OrthoPane::onLeftUp));
    this->Connect(wxEVT_SIZE, wxSizeEventHandler(OrthoPane::onResize));

    camera = controller->getCurrentCamera();
    particle.lastIdx = -1;

    const wxSize size = this->GetSize();
    arcBall.resize(Pixel(size.x, size.y));
}

OrthoPane::~OrthoPane() = default;

void OrthoPane::resetView() {
    dragging.initialMatrix = AffineMatrix::identity();
    camera->transform(AffineMatrix::identity());
}

void OrthoPane::onTimeStep(const Storage& storage, const Statistics& UNUSED(stats)) {
    if (controller->getParams().get<bool>(GuiSettingsId::CAMERA_AUTOSETUP)) {
        camera->autoSetup(storage);
    }
    if (ruler.mode != MeasurementMode::NONE) {
        this->updateRulerText();
    }
}

void OrthoPane::onPaint(wxPaintEvent& UNUSED(evt)) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);

    wxAutoBufferedPaintDC dc(this);
    const wxBitmap& bitmap = controller->getRenderedBitmap();
    if (!bitmap.IsOk()) {
        dc.Clear();
        return;
    }

    const bool drawShearingBox = controller->getParams().get<bool>(GuiSettingsId::SHOW_SHEARING_BOX) &&
                                 bool(controller->getShearingSheetView());
    const bool drawRuler = ruler.mode != MeasurementMode::NONE &&
                           (ruler.first || (ruler.mode == MeasurementMode::PARTICLE_PAIR && ruler.firstParticle));
    if (drawShearingBox || drawRuler) {
        wxImage image = bitmap.ConvertToImage();
        if (image.IsOk()) {
            if (drawShearingBox) {
                this->drawShearingBox(image);
            }
            if (drawRuler) {
                this->drawRuler(image);
            }
            dc.DrawBitmap(wxBitmap(image), wxPoint(0, 0));
            return;
        }
    }

    dc.DrawBitmap(bitmap, wxPoint(0, 0));
}

void OrthoPane::onMouseMotion(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    Pixel position(evt.GetPosition());
    if (evt.Dragging()) {
        Pixel offset = Pixel(position.x - dragging.position.x, -(position.y - dragging.position.y));
        if (evt.RightIsDown()) {
            AffineMatrix matrix = arcBall.drag(position, Vector(0._f));
            camera->transform(dragging.initialMatrix * matrix);
        } else {
            camera->pan(offset);
        }
        controller->refresh(camera->clone());
    } else if (ruler.mode == MeasurementMode::RULER && ruler.first && !ruler.second) {
        ruler.preview = this->getAnchor(position);
        this->updateRulerText();
        this->Refresh();
    }
    dragging.position = position;
}

void OrthoPane::onRightDown(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    arcBall.click(Pixel(evt.GetPosition()));
}

void OrthoPane::onRightUp(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    AffineMatrix matrix = arcBall.drag(Pixel(evt.GetPosition()), Vector(0._f));
    dragging.initialMatrix = dragging.initialMatrix * matrix;
}

void OrthoPane::onLeftUp(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    Pixel position(evt.GetPosition());
    if (ruler.mode == MeasurementMode::RULER) {
        if (Optional<Vector> anchor = this->getAnchor(position)) {
            if (!ruler.first || ruler.second) {
                ruler.first = anchor;
                ruler.second = NOTHING;
            } else {
                ruler.second = anchor;
            }
            ruler.preview = NOTHING;
            this->updateRulerText();
            this->Refresh();
        }
        return;
    }
    if (ruler.mode == MeasurementMode::PARTICLE_PAIR) {
        if (Optional<Size> selectedIdx = controller->getIntersectedParticle(position, 1.f)) {
            if (Optional<ParticleAnchor> anchor = this->getParticleAnchor(selectedIdx.value())) {
                if (!ruler.firstParticle || ruler.secondParticle) {
                    ruler.firstParticle = anchor;
                    ruler.secondParticle = NOTHING;
                } else {
                    ruler.secondParticle = anchor;
                }
                this->updateRulerText();
                this->Refresh();
            }
        }
        return;
    }

    Optional<Size> selectedIdx = controller->getIntersectedParticle(position, 1.f);
    if (selectedIdx.valueOr(-1) != particle.lastIdx.valueOr(-1)) {
        particle.lastIdx = selectedIdx;
        controller->setSelectedParticle(selectedIdx);
        controller->refresh(camera->clone());
    }
}

void OrthoPane::onMouseWheel(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    const float spin = evt.GetWheelRotation();
    const float amount = (spin > 0.f) ? 1.2f : 1.f / 1.2f;
    Pixel fixedPoint(evt.GetPosition());
    camera->zoom(fixedPoint, amount);
    controller->refresh(camera->clone());
    controller->setAutoZoom(false);
}

void OrthoPane::onResize(wxSizeEvent& evt) {
    const Pixel newSize(max(10, evt.GetSize().x), max(10, evt.GetSize().y));
    arcBall.resize(newSize);
    camera->resize(newSize);
    controller->tryRedraw();
}

void OrthoPane::setRulerEnabled(const bool enabled) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    ruler.mode = enabled ? MeasurementMode::RULER : MeasurementMode::NONE;
    ruler.first = NOTHING;
    ruler.second = NOTHING;
    ruler.preview = NOTHING;
    ruler.firstParticle = NOTHING;
    ruler.secondParticle = NOTHING;
    this->updateRulerText();
    this->Refresh();
}

void OrthoPane::setParticlePairEnabled(const bool enabled) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    ruler.mode = enabled ? MeasurementMode::PARTICLE_PAIR : MeasurementMode::NONE;
    ruler.first = NOTHING;
    ruler.second = NOTHING;
    ruler.preview = NOTHING;
    ruler.firstParticle = NOTHING;
    ruler.secondParticle = NOTHING;
    this->updateRulerText();
    this->Refresh();
}

void OrthoPane::setRulerUnits(const Float unitScale, const String& unitLabel) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    ruler.unitScale = unitScale;
    ruler.unitLabel = unitLabel;
    this->updateRulerText();
}

void OrthoPane::centerView() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    const Optional<OrthoViewSetup> setup =
        controller->getDisplayedOrthoSetup(camera->getFrame().row(2), camera->getSize());
    if (!setup) {
        return;
    }

    const Vector centerOfMass = setup->centerOfMass;
    const Vector delta = centerOfMass - camera->getTarget();
    camera->setPosition(camera->getPosition() + delta);
    camera->setTarget(centerOfMass);

    if (const Optional<float> currentWorldToPixel = camera->getWorldToPixel(centerOfMass)) {
        const Float ratio = setup->worldToPixel / currentWorldToPixel.value();
        if (ratio > EPS) {
            const Pixel center(camera->getSize().x / 2, camera->getSize().y / 2);
            camera->zoom(center, float(ratio));
        }
    }

    controller->refresh(camera->clone());
}

Optional<Vector> OrthoPane::getAnchor(const Pixel& position) const {
    return this->getPointOnViewPlane(position);
}

Optional<Vector> OrthoPane::getPointOnViewPlane(const Pixel& position) const {
    const Optional<CameraRay> ray = camera->unproject(Coords(position));
    if (!ray) {
        return NOTHING;
    }

    const Vector normal = getNormalized(camera->getTarget() - camera->getPosition());
    const Vector direction = ray->target - ray->origin;
    const Float denominator = dot(normal, direction);
    if (abs(denominator) < EPS) {
        return ray->origin;
    }

    const Float t = dot(normal, camera->getTarget() - ray->origin) / denominator;
    return ray->origin + t * direction;
}

Optional<OrthoPane::ParticleAnchor> OrthoPane::getParticleAnchor(const Size index) const {
    const Optional<Vector> position = controller->getDisplayedParticlePosition(index);
    if (!position) {
        return NOTHING;
    }

    ParticleAnchor anchor;
    anchor.position = position.value();
    if (Optional<Size> persistentIdx = controller->getDisplayedPersistentIndex(index)) {
        anchor.index = persistentIdx.value();
        anchor.usePersistentIndex = true;
    } else {
        anchor.index = index;
    }
    return anchor;
}

Optional<Vector> OrthoPane::resolveParticleAnchor(const Storage& storage, const ParticleAnchor& anchor) const {
    if (!storage.has(QuantityId::POSITION)) {
        return NOTHING;
    }

    ArrayView<const Vector> positions = storage.getValue<Vector>(QuantityId::POSITION);
    if (anchor.usePersistentIndex && storage.has(QuantityId::PERSISTENT_INDEX)) {
        ArrayView<const Size> persistentIdxs = storage.getValue<Size>(QuantityId::PERSISTENT_INDEX);
        auto iter = std::find(persistentIdxs.begin(), persistentIdxs.end(), anchor.index);
        if (iter != persistentIdxs.end()) {
            return positions[Size(iter - persistentIdxs.begin())];
        }
        return NOTHING;
    }
    if (anchor.index < positions.size()) {
        return positions[anchor.index];
    }
    return NOTHING;
}

void OrthoPane::updateParticleAnchor(const Storage& storage, Optional<ParticleAnchor>& anchor) {
    if (!anchor) {
        return;
    }
    if (Optional<Vector> position = this->resolveParticleAnchor(storage, anchor.value())) {
        anchor->position = position.value();
    } else {
        anchor = NOTHING;
    }
}

void OrthoPane::updateRulerText() {
    if (ruler.mode == MeasurementMode::NONE) {
        ruler.text = "Distance: --";
        this->notifyRulerTextChanged();
        return;
    }

    Optional<Vector> lhs;
    Optional<Vector> rhs;
    if (ruler.mode == MeasurementMode::RULER) {
        lhs = ruler.first;
        rhs = ruler.second ? ruler.second : ruler.preview;
    } else {
        lhs = ruler.firstParticle ? Optional<Vector>(ruler.firstParticle->position) : NOTHING;
        rhs = ruler.secondParticle ? Optional<Vector>(ruler.secondParticle->position) : NOTHING;
    }

    if (!lhs || !rhs) {
        ruler.text = "Distance: --";
        this->notifyRulerTextChanged();
        return;
    }

    const Float distance = getLength(rhs.value() - lhs.value());
    ruler.text = formatRulerDistance(distance, ruler.unitScale, ruler.unitLabel);
    this->notifyRulerTextChanged();
}

void OrthoPane::notifyRulerTextChanged() const {
    if (!onRulerTextChanged) {
        return;
    }
    const String text = ruler.text;
    if (isMainThread()) {
        onRulerTextChanged(text);
    } else {
        executeOnMainThread([callback = onRulerTextChanged, text] { callback(text); });
    }
}

void OrthoPane::drawShearingBox(wxImage& image) const {
    const Optional<ShearingSheetView> view = controller->getShearingSheetView();
    if (!view || !image.IsOk()) {
        return;
    }

    const Vector center = view->center;
    const Vector half = 0.5_f * view->boxSize;

    const int width = image.GetWidth();
    const int height = image.GetHeight();
    std::vector<uint8_t> outlineMask(Size(width * height), 0);
    std::vector<uint8_t> lineMask(Size(width * height), 0);

    const Vector cameraDir = getNormalized(camera->getTarget() - camera->getPosition());
    int axis = X;
    if (abs(cameraDir[Y]) > abs(cameraDir[axis])) {
        axis = Y;
    }
    if (abs(cameraDir[Z]) > abs(cameraDir[axis])) {
        axis = Z;
    }

    const int axisA = (axis + 1) % 3;
    const int axisB = (axis + 2) % 3;
    const Vector corners[4] = {
        center + Vector(
                     axisA == X ? -half[X] : axisB == X ? -half[X] : 0._f,
                     axisA == Y ? -half[Y] : axisB == Y ? -half[Y] : 0._f,
                     axisA == Z ? -half[Z] : axisB == Z ? -half[Z] : 0._f),
        center + Vector(
                     axisA == X ? +half[X] : axisB == X ? -half[X] : 0._f,
                     axisA == Y ? +half[Y] : axisB == Y ? -half[Y] : 0._f,
                     axisA == Z ? +half[Z] : axisB == Z ? -half[Z] : 0._f),
        center + Vector(
                     axisA == X ? +half[X] : axisB == X ? +half[X] : 0._f,
                     axisA == Y ? +half[Y] : axisB == Y ? +half[Y] : 0._f,
                     axisA == Z ? +half[Z] : axisB == Z ? +half[Z] : 0._f),
        center + Vector(
                     axisA == X ? -half[X] : axisB == X ? +half[X] : 0._f,
                     axisA == Y ? -half[Y] : axisB == Y ? +half[Y] : 0._f,
                     axisA == Z ? -half[Z] : axisB == Z ? +half[Z] : 0._f),
    };
    static const int edges[4][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 } };

    bool hasSegments = false;
    for (const auto& edge : edges) {
        const Optional<ProjectedPoint> p1 = camera->project(corners[edge[0]]);
        const Optional<ProjectedPoint> p2 = camera->project(corners[edge[1]]);
        if (!p1 || !p2) {
            continue;
        }
        hasSegments = true;
        markSegment(outlineMask, width, height, p1->coords, p2->coords, 3);
        markSegment(lineMask, width, height, p1->coords, p2->coords, 1);
    }

    if (!hasSegments) {
        return;
    }
    applyColorOverlay(image, outlineMask, 0, 0, 0);
    applyColorOverlay(image, lineMask, 255, 255, 255);
}

void OrthoPane::drawRuler(wxImage& image) const {
    Optional<Vector> lhs;
    Optional<Vector> rhs;
    if (ruler.mode == MeasurementMode::RULER) {
        lhs = ruler.first;
        rhs = ruler.second ? ruler.second : ruler.preview;
    } else if (ruler.mode == MeasurementMode::PARTICLE_PAIR) {
        lhs = ruler.firstParticle ? Optional<Vector>(ruler.firstParticle->position) : NOTHING;
        rhs = ruler.secondParticle ? Optional<Vector>(ruler.secondParticle->position) : NOTHING;
    } else {
        return;
    }

    if (!lhs || !image.IsOk()) {
        return;
    }

    const Optional<ProjectedPoint> firstProjected = camera->project(lhs.value());
    if (!firstProjected) {
        return;
    }

    const int width = image.GetWidth();
    const int height = image.GetHeight();
    std::vector<uint8_t> lineMask(Size(width * height), 0);
    std::vector<uint8_t> selectionMask(Size(width * height), 0);
    const int lineRadius = 2;
    const int capHalfLength = 8;
    const int capThickness = 2;
    auto markSelection = [&selectionMask, width, height](const ProjectedPoint& projected) {
        const int radius = max(4, int(std::ceil(projected.radius * 0.85f)));
        markDisc(selectionMask, width, height, projected.coords, radius);
    };

    if (ruler.mode == MeasurementMode::PARTICLE_PAIR) {
        markSelection(firstProjected.value());
    }

    if (rhs) {
        const Optional<ProjectedPoint> secondProjected = camera->project(rhs.value());
        if (secondProjected) {
            const Coords p1 = firstProjected->coords;
            const Coords p2 = secondProjected->coords;
            Coords direction = p2 - p1;
            if (getLength(direction) < 1.e-3f) {
                direction = Coords(1.f, 0.f);
            }
            markSegment(lineMask, width, height, p1, p2, lineRadius);
            markCap(lineMask, width, height, p1, direction, capHalfLength, capThickness);
            markCap(lineMask, width, height, p2, direction, capHalfLength, capThickness);
            if (ruler.mode == MeasurementMode::PARTICLE_PAIR) {
                markSelection(secondProjected.value());
            }
        }
    } else {
        markCap(lineMask, width, height, firstProjected->coords, Coords(1.f, 0.f), capHalfLength, capThickness);
    }

    applyContrastOverlay(image, lineMask);
    applyColorOverlay(image, selectionMask, 255, 0, 0);
}

NAMESPACE_SPH_END
