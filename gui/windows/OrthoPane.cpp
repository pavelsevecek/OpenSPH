#include "gui/windows/OrthoPane.h"
#include "gui/Factory.h"
#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Bitmap.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include <wx/dcclient.h>

NAMESPACE_SPH_BEGIN

OrthoPane::OrthoPane(wxWindow* parent, const std::shared_ptr<Storage>& storage, const GuiSettings& settings)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    , storage(storage)
    , settings(settings) {
    this->SetMinSize(wxSize(640, 480));
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(OrthoPane::onPaint));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(OrthoPane::onMouseMotion));
    this->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(OrthoPane::onMouseWheel));
    this->Connect(wxEVT_TIMER, wxTimerEventHandler(OrthoPane::onTimer));
    camera = Factory::getCamera(settings);
    setQuantity(QuantityIds::POSITIONS);
    refreshTimer = new wxTimer(this, 1); /// \todo check that timer is destroyed
    refreshTimer->Start(50);
}

OrthoPane::~OrthoPane() = default;

void OrthoPane::onPaint(wxPaintEvent& UNUSED(evt)) {
    MEASURE_SCOPE("OrthoPane::onPaint");
    // called from main thread
    wxPaintDC dc(this);
    bitmap.Create(dc.GetSize(), 24);
    wxMemoryDC memoryDc(bitmap);
    if (particlesUpdated) {
        std::unique_lock<std::mutex> lock(mutex);
        /// \todo maybe just lock the whole functions and don't complicate things with BufferedArray
        particles.swap();
        particlesUpdated = false;
    }
    memoryDc.SetBrush(*wxBLACK_BRUSH);
    memoryDc.DrawRectangle(wxPoint(0, 0), dc.GetSize());
    const float scale = settings.get<Float>(GuiSettingsIds::PARTICLE_RADIUS);
    wxBrush brush(*wxBLACK_BRUSH);
    wxPen pen(*wxBLACK_PEN);
    for (Particle& p : particles.second()) {
        brush.SetColour(p.color);
        pen.SetColour(p.color);
        memoryDc.SetBrush(brush);
        memoryDc.SetPen(pen);
        const float radius = camera->projectedSize(p.position, scale * p.position[H]).get();
        memoryDc.DrawCircle(camera->project(p.position).get(), radius);
    }
    drawPalette(memoryDc);
    dc.DrawBitmap(bitmap, wxPoint(0, 0));
    dc.DrawText(("t = " + std::to_string(time) + "s").c_str(), wxPoint(0, 0));
}

void OrthoPane::drawPalette(wxDC& dc) {
    const int size = 201;
    wxPoint origin(dc.GetSize().x - 50, size + 30);
    wxPen pen = dc.GetPen();
    for (int i = 0; i < size; ++i) {
        Palette palette = element->getPalette();
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
            const std::string text = ss.str();
            wxSize extent = dc.GetTextExtent(text);
            dc.DrawText(text, wxPoint(origin.x - 60, origin.y - i - (extent.y >> 1)));
        }
    }
}

void OrthoPane::onMouseMotion(wxMouseEvent& evt) {
    Point position = evt.GetPosition();
    Point offset;
    if (evt.Dragging()) {
        offset = Point(position.x - lastMousePosition.x, -(position.y - lastMousePosition.y));
        camera->pan(offset);
        this->Refresh();
    }
    lastMousePosition = position;
    evt.Skip();
}

void OrthoPane::onMouseWheel(wxMouseEvent& evt) {
    const float spin = evt.GetWheelRotation();
    const float amount = (spin > 0.f) ? 1.2f : 1.f / 1.2f;
    camera->zoom(amount);
    this->Refresh();
    evt.Skip();
}

void OrthoPane::onTimer(wxTimerEvent& evt) {
    this->Refresh();
    evt.Skip();
}

void OrthoPane::draw(const std::shared_ptr<Storage>& newStorage, const Statistics& stats) {
    MEASURE_SCOPE("OrthoPane::draw");
    // called from worker thread, cannot touch wx stuff here
    mutex.lock();
    storage = newStorage;
    time = stats.get<Float>(StatisticsIds::TIME);
    mutex.unlock();
    update();
}

Bitmap OrthoPane::getRender() const {
    return bitmap;
}

void OrthoPane::update() {
    MEASURE_SCOPE("OrthoPane::update");
    ASSERT(storage);
    std::unique_lock<std::mutex> lock(mutex);
    ArrayView<Vector> r = storage->getValue<Vector>(QuantityIds::POSITIONS);
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

void OrthoPane::setQuantity(const QuantityIds key) {
    quantity = key;
    update();
    this->Refresh();
}


NAMESPACE_SPH_END
