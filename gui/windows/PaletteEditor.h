#pragma once

#include "gui/objects/Color.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Optional.h"
#include <iostream>
#include <wx/colordlg.h>
#include <wx/dcclient.h>
#include <wx/frame.h>

NAMESPACE_SPH_BEGIN

class PaletteEditor : public wxPanel {
private:
    struct Point {
        float position;
        Rgba color;
    };

    Array<Point> points;

    Optional<Size> active;

public:
    PaletteEditor(wxWindow* parent, wxSize size)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, size) {
        this->SetMinSize(wxSize(320, 100));

        this->Connect(wxEVT_PAINT, wxPaintEventHandler(PaletteEditor::onPaint));
        this->Connect(wxEVT_MOTION, wxMouseEventHandler(PaletteEditor::onMouseMotion));
        this->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(PaletteEditor::onLeftDown));
        this->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(PaletteEditor::onLeftUp));
        this->Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(PaletteEditor::onDoubleClick));
        this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(PaletteEditor::onRightUp));

        points.push(Point{ 0.f, Rgba::black() });
        points.push(Point{ 0.5f, Rgba::red() });
        points.push(Point{ 1.f, Rgba::white() });
    }

private:
    const wxSize margin = wxSize(10, 10);

    void onPaint(wxPaintEvent& UNUSED(evt)) {
        wxPaintDC dc(this);

        const wxSize size = dc.GetSize() - 2 * margin;
        wxPen pen = dc.GetPen();
        const bool enabled = this->IsEnabled();
        for (int x = 0; x < size.x; ++x) {
            const float pos = float(x) / size.x;
            const Rgba color = enabled ? getColor(pos) : Rgba::gray(0.4f);
            pen.SetColour(wxColour(color));
            dc.SetPen(pen);
            dc.DrawLine(wxPoint(x + margin.x, margin.y), wxPoint(x + margin.x, margin.y + size.y));
        }

        pen.SetColour(*wxBLACK);
        dc.SetPen(pen);
        if (!enabled) {
            wxBrush brush = dc.GetBrush();
            brush.SetColour(wxColour(160, 160, 160));
            dc.SetBrush(brush);
        }
        const int thickness = 4;
        const int overhang = 6;
        for (const Point& p : points) {
            const int x = pointToWindow(p.position);
            dc.DrawRectangle(
                wxPoint(x - thickness / 2, margin.y - overhang), wxSize(thickness, size.y + 2 * overhang));
        }
    }

    Rgba getColor(const float pos) const {
        auto iter = std::lower_bound(points.begin(), points.end(), pos, [](const Point& p, const float pos) {
            return p.position < pos;
        });
        if (iter == points.begin()) {
            return points.front().color;
        } else if (iter == points.end()) {
            return points.back().color;
        } else {
            const Rgba color2 = iter->color;
            const Rgba color1 = (iter - 1)->color;
            const float pos2 = iter->position;
            const float pos1 = (iter - 1)->position;
            const float x = (pos - pos1) / (pos2 - pos1);
            return lerp(color1, color2, x);
        }
    }

    Optional<Size> lock(const int x) const {
        for (Size i = 0; i < points.size(); ++i) {
            const int p = pointToWindow(points[i].position);
            if (abs(x - p) < 10) {
                return i;
            }
        }
        return NOTHING;
    }

    float windowToPoint(const int x) const {
        const int width = this->GetSize().x - 2 * margin.x;
        return float(x - margin.x) / width;
    }

    int pointToWindow(const float x) const {
        const int width = this->GetSize().x - 2 * margin.x;
        return x * width + margin.x;
    }

    void onMouseMotion(wxMouseEvent& evt) {
        if (!active) {
            return;
        }
        const int x = evt.GetPosition().x;
        const Size index = active.value();
        points[index].position = clamp(windowToPoint(x), 0.f, 1.f);
        if (index > 0 && points[index].position < points[index - 1].position) {
            std::swap(points[index], points[index - 1]);
            active = index - 1;
        } else if (index < points.size() - 1 && points[index].position > points[index + 1].position) {
            std::swap(points[index], points[index + 1]);
            active = index + 1;
        }

        this->Refresh();
    }

    void onLeftUp(wxMouseEvent& UNUSED(evt)) {
        active = NOTHING;
    }

    void onLeftDown(wxMouseEvent& evt) {
        active = this->lock(evt.GetPosition().x);
    }

    void onRightUp(wxMouseEvent& evt) {
        const Optional<Size> index = this->lock(evt.GetPosition().x);
        if (index && index.value() > 0 && index.value() < points.size() - 1) {
            points.remove(index.value());
            this->Refresh();
        }
    }

    void onDoubleClick(wxMouseEvent& evt) {
        Optional<Size> index = this->lock(evt.GetPosition().x);
        if (!index) {
            const float pos = windowToPoint(evt.GetPosition().x);
            for (Size i = 0; i < points.size(); ++i) {
                if (points[i].position > pos) {
                    index = i;
                    break;
                }
            }
            if (!index) {
                index = points.size();
            }
            points.insert(index.value(), Point{ pos, getColor(pos) });
        }

        wxColourDialog* dialog = new wxColourDialog(this);
        dialog->GetColourData().SetColour(wxColour(points[index.value()].color));

        if (dialog->ShowModal() == wxID_OK) {
            const wxColourData& data = dialog->GetColourData();
            points[index.value()].color = Rgba(data.GetColour());
        }
        this->Refresh();
    }
};

class PaletteEditorFrame : public wxFrame {
public:
    PaletteEditorFrame(wxWindow* parent, wxSize size)
        : wxFrame(parent, wxID_ANY, "test", wxDefaultPosition, size) {
        new PaletteEditor(this, size);
    }
};

NAMESPACE_SPH_END
