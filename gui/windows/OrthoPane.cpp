#include "gui/windows/OrthoPane.h"
#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Element.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"
#include <wx/dcclient.h>
#include <wx/timer.h>

NAMESPACE_SPH_BEGIN

Bitmap OrthoRenderer::render(ArrayView<const Vector> r,
    Abstract::Element& element,
    const RenderParams& params,
    Statistics& stats) const {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    MEASURE_SCOPE("OrthoRenderer::render");
    const wxSize size(params.size.x, params.size.y);
    wxBitmap bitmap(size, 24);
    wxMemoryDC dc(bitmap);

    // draw black background (there is no fill method?)
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(wxPoint(0, 0), size);
    wxBrush brush(*wxBLACK_BRUSH);
    wxPen pen(*wxBLACK_PEN);
    for (Size i = 0; i < r.size(); ++i) {
        const Color color = element.eval(i);
        brush.SetColour(color);
        pen.SetColour(color);
        dc.SetBrush(brush);
        dc.SetPen(pen);
        const Optional<Tuple<Point, float>> p = params.camera->project(r[i]);
        if (p) {
            const int size = max(int(p->get<float>() * params.particles.scale), 1);
            dc.DrawCircle(p->get<Point>(), size);
        }
    }
    this->drawPalette(dc, element.getPalette());
    const Float time = stats.get<Float>(StatisticsId::TOTAL_TIME);
    dc.DrawText(("t = " + std::to_string(time) + "s").c_str(), wxPoint(0, 0));

    dc.SelectObject(wxNullBitmap);
    return bitmap;
}

void OrthoRenderer::drawPalette(wxDC& dc, const Palette& palette) const {
    const int size = 201;
    wxPoint origin(dc.GetSize().x - 50, size + 30);
    wxPen pen = dc.GetPen();
    for (Size i = 0; i < size; ++i) {
        const float value = palette.getInterpolatedValue(float(i) / (size - 1));
        wxColour color = palette(value);
        pen.SetColour(color);
        dc.SetPen(pen);
        dc.DrawLine(wxPoint(origin.x, origin.y - i), wxPoint(origin.x + 30, origin.y - i));
        if (i % 50 == 0) {
            dc.SetTextForeground(Color::white());
            wxFont font(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
            dc.SetFont(font);
            std::stringstream ss;
            ss << std::setprecision(1) << std::scientific << value;
            const wxString text = ss.str();
            wxSize extent = dc.GetTextExtent(text);
            dc.DrawText(text, wxPoint(origin.x - 60, origin.y - i - (extent.y >> 1)));
        }
    }
}

OrthoPane::OrthoPane(wxWindow* parent, Controller* controller)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    , controller(controller)
    , camera(controller->getCamera()) {
    this->SetMinSize(wxSize(640, 480));
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(OrthoPane::onPaint));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(OrthoPane::onMouseMotion));
    this->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(OrthoPane::onMouseWheel));
    this->Connect(wxEVT_TIMER, wxTimerEventHandler(OrthoPane::onTimer));

    refreshTimer = makeAuto<wxTimer>(this, 1);
    refreshTimer->Start(50);
}

OrthoPane::~OrthoPane() = default;

void OrthoPane::setElement(AutoPtr<Abstract::Element>&&) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    // element = std::move(newElement);
    // this->update()
}

void OrthoPane::onPaint(wxPaintEvent& UNUSED(evt)) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    MEASURE_SCOPE("OrthoPane::onPaint");
    wxPaintDC dc(this);
    Bitmap bitmap = controller->getRenderedBitmap();
    if (bitmap.isOk()) { // not empty
        dc.DrawBitmap(bitmap, wxPoint(0, 0));
    }
}

void OrthoPane::onMouseMotion(wxMouseEvent& evt) {
    Point position = evt.GetPosition();
    if (evt.Dragging()) {
        Point offset = Point(position.x - dragging.position.x, -(position.y - dragging.position.y));
        ASSERT(camera);
        camera->pan(offset);
        this->Refresh();
    }
    dragging.position = position;
    evt.Skip();
}

void OrthoPane::onMouseWheel(wxMouseEvent& evt) {
    const float spin = evt.GetWheelRotation();
    const float amount = (spin > 0.f) ? 1.2f : 1.f / 1.2f;
    ASSERT(camera);
    camera->zoom(amount);
    this->Refresh();
    evt.Skip();
}

void OrthoPane::onTimer(wxTimerEvent& evt) {
    wxYield();
    this->Refresh();
    evt.Skip();
}


/*void OrthoPane::draw(const SharedPtr<Storage>& newStorage, const Statistics& stats) {
    MEASURE_SCOPE("OrthoPane::draw");
    // called from worker thread, cannot touch wx stuff here
    mutex.lock();
    storage = newStorage;
    time = stats.get<Float>(StatisticsId::TOTAL_TIME);
    mutex.unlock();
    update();
}

Bitmap OrthoPane::getRender() const {
    return bitmap;
}*/

/*void OrthoPane::update() {
    MEASURE_SCOPE("OrthoPane::update");
    ASSERT(storage);
    std::unique_lock<std::mutex> lock(mutex);
    ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITIONS);
    particles->clear();
    element = Factory::getElement(*storage, settings, quantity);
    for (Size i = 0; i < r.size(); ++i) {
        Optional<Point> point = camera->project(r[i]);
        if (!point) {
            continue;
        }
        Particle p{ r[i], element->eval(i) };
        particles->push(p);
    }
    particlesUpdated = true;
}

void OrthoPane::setQuantity(const QuantityId key) {
    quantity = key;
    if (storage) {
        this->update();
    }
    this->Refresh();
}
*/

NAMESPACE_SPH_END
