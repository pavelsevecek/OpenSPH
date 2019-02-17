#include "gui/windows/PlotView.h"
#include "gui/Utils.h"
#include "gui/objects/SvgContext.h"
#include "io/Logger.h"
#include "io/Path.h"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dcclient.h>
#include <wx/menu.h>
#include <wx/sizer.h>

NAMESPACE_SPH_BEGIN

PlotView::PlotView(wxWindow* parent,
    const wxSize size,
    const wxSize padding,
    const SharedPtr<Array<PlotData>>& list,
    const Size defaultSelectedIdx,
    const bool showLabels)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
    , padding(padding)
    , list(list)
    , showLabels(showLabels) {
    this->SetMaxSize(size);
    Connect(wxEVT_PAINT, wxPaintEventHandler(PlotView::onPaint));
    Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(PlotView::onRightUp));
    Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(PlotView::onDoubleClick));

    this->updatePlot(defaultSelectedIdx);
}

void PlotView::resize(const Pixel size) {
    //    this->SetMaxSize(wxSize(size.x, size.y));
    this->SetSize(wxSize(size.x, size.y));
}

static Interval extendRange(const Interval& range, const bool addZero) {
    Interval actRange = range;
    if (addZero) {
        const Float eps = 0.05_f * range.size();
        actRange.extend(eps);
        actRange.extend(-eps);
    }
    return actRange;
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
    PlotFrame* frame = new PlotFrame(this->GetParent(), wxSize(800, 600), wxSize(25, 25), cached.plot);
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
    Rgba backgroundColor = Rgba(this->GetParent()->GetBackgroundColour());
    wxBrush brush;
    brush.SetColour(wxColour(backgroundColor.darken(0.2_f)));
    dc.SetBrush(brush);
    dc.DrawRectangle(wxPoint(0, 0), canvasSize);

    auto proxy = cached.plot.lock();
    this->drawCaption(dc, *proxy);

    const Interval rangeX = extendRange(proxy->rangeX(), addZeroX);
    const Interval rangeY = extendRange(proxy->rangeY(), addZeroY);
    if (rangeX.size() <= 0._f || rangeY.size() <= 0) {
        // don't assert, it probably means there are no data to draw
        return;
    }

    wxPen pen;
    pen.SetColour(*wxWHITE);
    dc.SetPen(pen);
    this->drawAxes(dc, rangeX, rangeY);
    this->drawPlot(dc, *proxy, rangeX, rangeY);
}

void PlotView::drawPlot(wxPaintDC& dc, IPlot& lockedPlot, const Interval rangeX, const Interval rangeY) {
    GraphicsContext context(dc, cached.color);
    const AffineMatrix2 matrix = this->getPlotTransformMatrix(rangeX, rangeY);
    context.setTransformMatrix(matrix);

    lockedPlot.plot(context);
}

void PlotView::drawAxes(wxDC& dc, const Interval rangeX, const Interval rangeY) {
    const wxSize size = this->GetSize();
    const AffineMatrix2 matrix = this->getPlotTransformMatrix(rangeX, rangeY);

    // find point where y-axis appears on the polot
    const Float x0 = -rangeX.lower() / rangeX.size();
    if (x0 >= 0._f && x0 <= 1._f) {
        // draw y-axis
        const Float dcX = padding.x + x0 * (size.x - 2 * padding.x);
        dc.DrawLine(dcX, size.y - padding.y, dcX, padding.y);
        if (showLabels) {
            Array<Float> tics = getLinearTics(rangeY, 6);
            ASSERT(tics.size() >= 6);
            for (const Float tic : tics) {
                const PlotPoint plotPoint(0, tic);
                const PlotPoint imagePoint = matrix.transformPoint(plotPoint);
                dc.DrawLine(imagePoint.x - 2, imagePoint.y, imagePoint.x + 2, imagePoint.y);
                const std::wstring text = toPrintableString(tic, 3);
                const wxSize extent = dc.GetTextExtent(text);
                const Float labelX = (imagePoint.x > size.x / 2._f) ? imagePoint.x - extent.x : imagePoint.x;
                drawTextWithSubscripts(dc, text, wxPoint(labelX, imagePoint.y - extent.y / 2));
            }
        }
    }
    // find point where x-axis appears on the plot
    const Float y0 = -rangeY.lower() / rangeY.size();
    if (y0 >= 0._f && y0 <= 1._f) {
        // draw x-axis
        const Float dcY = size.y - padding.y - y0 * (size.y - 2 * padding.y);
        dc.DrawLine(padding.x, dcY, size.x - padding.x, dcY);
        if (showLabels) {
            Array<Float> tics = getLinearTics(rangeX, 6);
            for (const Float tic : tics) {
                const PlotPoint plotPoint(tic, 0);
                const PlotPoint imagePoint = matrix.transformPoint(plotPoint);
                dc.DrawLine(imagePoint.x, imagePoint.y - 2, imagePoint.x, imagePoint.y + 2);
                const std::wstring text = toPrintableString(tic, 3);
                const wxSize extent = dc.GetTextExtent(text);
                const Float labelY = (imagePoint.y < size.y / 2._f) ? imagePoint.y : imagePoint.y - extent.y;
                drawTextWithSubscripts(dc, text, wxPoint(imagePoint.x - extent.x / 2, labelY));
            }
        }
    }
}

void PlotView::drawCaption(wxDC& dc, IPlot& lockedPlot) {
    // plot may change caption during simulation (by selecting particle, for example), so we need to get the
    // name every time from the plot
    const wxString label = lockedPlot.getCaption();
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
    data->push(PlotData{ plot, Rgba(0.7f, 0.7f, 0.7f) });

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    const Size toolbarHeight = 20;
    wxBoxSizer* toolbarSizer = this->createToolbar(toolbarHeight);
    sizer->Add(toolbarSizer);

    wxSize viewSize(size.x, size.y - toolbarHeight);
    plotView = new PlotView(this, viewSize, padding, data, 0, true);
    sizer->Add(plotView);
    this->SetSizerAndFit(sizer);

    this->Bind(wxEVT_SIZE, [this](wxSizeEvent& evt) {
        const wxSize size = evt.GetSize();
        plotView->resize(Pixel(size.x, size.y - toolbarHeight));
    });
}

wxBoxSizer* PlotFrame::createToolbar(const Size UNUSED(toolbarHeight)) {
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

    wxButton* savePlotButton = new wxButton(this, wxID_ANY, "Save Plot");
    savePlotButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        Optional<Path> path =
            doSaveFileDialog("Save image", { { "PNG image", "png" }, { "SVG image", "svg" } });
        if (path) {
            this->saveImage(path.value());
        }
    });
    sizer->Add(savePlotButton, 0, wxALIGN_CENTER_VERTICAL);

    wxButton* saveDataButton = new wxButton(this, wxID_ANY, "Save Data");
    saveDataButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        Optional<Path> path = doSaveFileDialog("Save data", { { "Text file", "txt" } });
        if (path) {
            this->saveData(path.value());
        }
    });
    sizer->Add(saveDataButton, 0, wxALIGN_CENTER_VERTICAL);

    wxButton* refreshButton = new wxButton(this, wxID_ANY, "Refresh");
    refreshButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->Refresh(); });
    sizer->Add(refreshButton, 0, wxALIGN_CENTER_VERTICAL);

    wxCheckBox* addZeroXBox = new wxCheckBox(this, wxID_ANY, "Show zero X");
    addZeroXBox->Bind(wxEVT_CHECKBOX, [this, addZeroXBox](wxCommandEvent& UNUSED(evt)) {
        const bool checked = addZeroXBox->GetValue();
        plotView->addZeroX = checked;
        this->Refresh();
    });
    sizer->Add(addZeroXBox, 0, wxALIGN_CENTER_VERTICAL);

    wxCheckBox* addZeroYBox = new wxCheckBox(this, wxID_ANY, "Show zero Y");
    addZeroYBox->Bind(wxEVT_CHECKBOX, [this, addZeroYBox](wxCommandEvent& UNUSED(evt)) {
        const bool checked = addZeroYBox->GetValue();
        plotView->addZeroY = checked;
        this->Refresh();
    });
    sizer->Add(addZeroYBox, 0, wxALIGN_CENTER_VERTICAL);
    return sizer;
}

void PlotFrame::saveImage(const Path& path) {
    if (path.extension() == Path("png")) {
        wxBitmap bitmap(800, 600, wxBITMAP_SCREEN_DEPTH);
        wxMemoryDC dc(bitmap);
        dc.SetBrush(*wxWHITE_BRUSH);
        dc.DrawRectangle(0, 0, 800, 600);

        auto proxy = plot.lock();
        GraphicsContext gc(dc, Rgba(0.f, 0.f, 0.5f));
        const Interval actRangeX = extendRange(proxy->rangeX(), plotView->addZeroX);
        const Interval actRangeY = extendRange(proxy->rangeY(), plotView->addZeroY);
        AffineMatrix2 matrix = plotView->getPlotTransformMatrix(actRangeX, actRangeY);
        gc.setTransformMatrix(matrix);
        proxy->plot(gc);

        /// \todo refactor, move labels and axes to IPlot
        wxPen pen;
        pen.SetColour(*wxBLACK);
        dc.SetPen(pen);
        plotView->drawAxes(dc, actRangeX, actRangeY);

        dc.SelectObject(wxNullBitmap);

        bitmap.SaveFile(path.native().c_str(), wxBITMAP_TYPE_PNG);
    } else if (path.extension() == Path("svg")) {
        auto proxy = plot.lock();
        SvgContext gc(path, Pixel(800, 600));
        AffineMatrix2 matrix = plotView->getPlotTransformMatrix(proxy->rangeX(), proxy->rangeY());
        gc.setTransformMatrix(matrix);
        proxy->plot(gc);
    } else {
        NOT_IMPLEMENTED;
    }
}

class TextContext : public IDrawingContext {
private:
    FileLogger logger;

public:
    explicit TextContext(const Path& path)
        : logger(path) {}

    virtual void drawPoint(const PlotPoint& point) override {
        logger.write(point.x, "  ", point.y);
    }

    virtual void drawErrorPoint(const ErrorPlotPoint& point) override {
        logger.write(point.x, "  ", point.y);
    }

    virtual void drawLine(const PlotPoint& UNUSED(from), const PlotPoint& UNUSED(to)) override {
        NOT_IMPLEMENTED;
    }

    class TextPath : public IDrawPath {
    public:
        virtual void addPoint(const PlotPoint& UNUSED(point)) override {
            // N/A
        }

        virtual void closePath() override {
            // N/A
        }

        virtual void endPath() override {
            // N/A
        }
    };

    virtual AutoPtr<IDrawPath> drawPath() override {
        return makeAuto<TextPath>();
    }

    virtual void setStyle(const Size UNUSED(index)) override {
        // possibly new plot, separate by newline
        logger.write("");
    }

    virtual void setTransformMatrix(const AffineMatrix2& UNUSED(matrix)) override {
        // not applicable for text output
    }
};

void PlotFrame::saveData(const Path& path) {
    ASSERT(path.extension() == Path("txt"));
    auto proxy = plot.lock();
    TextContext context(path);
    proxy->plot(context);
}

NAMESPACE_SPH_END
