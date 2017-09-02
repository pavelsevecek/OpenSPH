#include "gui/windows/PlotView.h"
#include "gui/objects/SvgContext.h"
#include "io/Path.h"
#include <wx/button.h>
#include <wx/dcclient.h>
#include <wx/filedlg.h>
#include <wx/menu.h>
#include <wx/sizer.h>

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

AffineMatrix2 PlotView::getPlotTransformMatrix(const Interval& rangeX, const Interval& rangeY) const {
    // actual size of the plot
    const wxSize size = this->GetSize() - 2 * padding;

    // scaling factors
    const Float scaleX = size.x / rangeX.size();
    const Float scaleY = -size.y / rangeY.size();

    // translation
    const Float transX = padding.x - scaleX * rangeX.lower();
    const Float transY = size.y + padding.y - scaleY * rangeY.lower();

    return AffineMatrix2(scaleX, 0._f, 0._f, scaleY, transX, transY);
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
    GraphicsContext context(dc, cached.color);
    const AffineMatrix2 matrix = this->getPlotTransformMatrix(rangeX, rangeY);
    context.setTransformMatrix(matrix);

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
    , plot(plot)
    , padding(padding) {
    this->SetMinSize(size);
    SharedPtr<Array<PlotData>> data = makeShared<Array<PlotData>>();
    data->push(PlotData{ plot, Color(0.7f, 0.7f, 1.f) });

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxButton* saveButton = new wxButton(this, wxID_ANY, "Save");
    saveButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        wxFileDialog saveFile(this,
            "Save image",
            "",
            "",
            "PNG image (*.png)|*.png|SVG image (*.svg)|*.svg",
            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveFile.ShowModal() == wxID_OK) {
            this->saveImage(std::string(saveFile.GetPath()), saveFile.GetFilterIndex());
        }
    });
    sizer->Add(saveButton);

    wxSize viewSize(size.x, size.y - saveButton->GetSize().y);
    plotView = new PlotView(this, viewSize, padding, data, 0);
    sizer->Add(plotView);
    this->SetSizer(sizer);
}

void PlotFrame::saveImage(const std::string& pathStr, const int fileIndex) {
    Path path(pathStr);

    switch (fileIndex) {
    case 0: { // png file
        path.replaceExtension("png");

        wxBitmap bitmap(800, 600, wxBITMAP_SCREEN_DEPTH);
        wxMemoryDC dc(bitmap);
        dc.SetBrush(*wxWHITE_BRUSH);
        dc.DrawRectangle(0, 0, 800, 600);

        auto proxy = plot.lock();
        GraphicsContext gc(dc, Color(0.f, 0.f, 0.5f));
        AffineMatrix2 matrix = plotView->getPlotTransformMatrix(proxy->rangeX(), proxy->rangeY());
        gc.setTransformMatrix(matrix);
        proxy->plot(gc);
        dc.SelectObject(wxNullBitmap);

        bitmap.SaveFile(path.native().c_str(), wxBITMAP_TYPE_PNG);
        break;
    }
    case 1: { // svg file
        path.replaceExtension("svg");

        auto proxy = plot.lock();
        SvgContext gc(path, Point(800, 600));
        AffineMatrix2 matrix = plotView->getPlotTransformMatrix(proxy->rangeX(), proxy->rangeY());
        gc.setTransformMatrix(matrix);
        proxy->plot(gc);
    }
    }
}

NAMESPACE_SPH_END
