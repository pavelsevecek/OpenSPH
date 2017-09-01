#include "gui/windows/PlotView.h"
#include <wx/dcclient.h>
#include <wx/menu.h>

NAMESPACE_SPH_BEGIN

PlotView::PlotView(wxWindow* parent,
    const wxSize size,
    const wxSize padding,
    const SharedPtr<Array<PlotData>>& list,
    const Size defaultSelectedIdx)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
    , padding(padding)
    , list(list) {
    this->SetMaxSize(size);
    Connect(wxEVT_PAINT, wxPaintEventHandler(PlotView::onPaint));
    Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(PlotView::onRightUp));
    Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(PlotView::onDoubleClick));

    this->updatePlot(defaultSelectedIdx);
}

void PlotView::updatePlot(const Size index) {
    PlotData& data = (*list)[index];
    cached.name = data.plot->getCaption();
    cached.color = data.color;

    // plot needs to be synchronized as it is updated from different thread, hopefully neither updating
    // nor drawing will take a lot of time, so we can simply lock the pointer.
    cached.plot = data.plot;
}

void PlotView::onRightUp(wxMouseEvent& UNUSED(evt)) {
    if (list->size() <= 1) {
        // nothing to choose from, do nothing
        return;
    }

    wxMenu menu;
    Size index = 0;
    for (PlotData& data : *list) {
        menu.Append(index++, data.plot->getCaption());
    }

    menu.Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(PlotView::onMenu), nullptr, this);
    this->PopupMenu(&menu);
}

void PlotView::onDoubleClick(wxMouseEvent& UNUSED(evt)) {
    PlotFrame* frame = new PlotFrame(this->GetParent(), wxSize(800, 600), padding, cached.plot);
    frame->Show();
}

void PlotView::onMenu(wxCommandEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    const Size index = evt.GetId();
    ASSERT(index < list->size());
    this->updatePlot(index);
    this->Refresh();
}

void PlotView::onPaint(wxPaintEvent& UNUSED(evt)) {
    wxPaintDC dc(this);
    wxSize canvasSize = dc.GetSize();

    // draw background
    Color backgroundColor = Color(this->GetParent()->GetBackgroundColour());
    wxBrush brush;
    brush.SetColour(wxColour(backgroundColor.darken(0.2_f)));
    dc.SetBrush(brush);
    dc.DrawRectangle(wxPoint(0, 0), canvasSize);

    this->drawLabel(dc);

    auto proxy = cached.plot.lock();
    const Interval rangeX = proxy->rangeX();
    const Interval rangeY = proxy->rangeY();
    if (rangeX.size() <= 0._f || rangeY.size() <= 0) {
        // don't assert, it probably means there are no data to draw
        return;
    }
    this->drawAxes(dc, rangeX, rangeY);
    this->drawPlot(dc, *proxy, rangeX, rangeY);
}

void PlotView::drawPlot(wxPaintDC& dc, IPlot& lockedPlot, const Interval rangeX, const Interval rangeY) {
    WxDrawingContext context(dc, padding, rangeX, rangeY, cached.color);
    lockedPlot.plot(context);
}

void PlotView::drawAxes(wxPaintDC& dc, const Interval rangeX, const Interval rangeY) {
    wxPen pen;
    pen.SetColour(*wxWHITE);
    dc.SetPen(pen);
    const wxSize size = this->GetSize();
    // find point where y-axis appears on the polot
    const Float x0 = -rangeX.lower() / rangeX.size();
    if (x0 >= 0._f && x0 <= 1._f) {
        // draw y-axis
        const Float dcX = x0 * (size.x - 2 * padding.x);
        dc.DrawLine(padding.x + dcX, size.y - padding.y, padding.x + dcX, padding.y);
    }
    // find point where x-axis appears on the plot
    const Float y0 = -rangeY.lower() / rangeY.size();
    if (y0 >= 0._f && y0 <= 1._f) {
        // draw x-axis
        const Float dcY = y0 * (size.y - 2 * padding.y);
        dc.DrawLine(padding.x, size.y - padding.y - dcY, size.x - padding.x, size.y - padding.y - dcY);
    }
}

void PlotView::drawLabel(wxDC& dc) {
    const wxString label = cached.name;
    wxFont font = dc.GetFont();
    font.MakeSmaller();
    dc.SetFont(font);
    const wxSize labelSize = dc.GetTextExtent(label);
    dc.DrawText(label, dc.GetSize().x - labelSize.x, 0);
}

PlotFrame::PlotFrame(wxWindow* parent, const wxSize size, const wxSize padding, const LockingPtr<IPlot>& plot)
    : wxFrame(parent, wxID_ANY, "Plot")
    , plot(plot) {
    this->SetMinSize(size);
    SharedPtr<Array<PlotData>> data = makeShared<Array<PlotData>>();
    data->push(PlotData{ plot, Color(1.f, 0.3f, 0.5f) });
    /*PlotView* largeView =*/new PlotView(this, size, padding, data, 0);

    wxButton* saveButton = new wxButton(this, wxID_ANY, ...);
}


NAMESPACE_SPH_END
