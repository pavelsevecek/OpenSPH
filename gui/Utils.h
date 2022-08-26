#pragma once

/// \file Utils.h
/// \brief Random utility functions for drawing stuff to DC
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/Globals.h"
#include "gui/renderers/IRenderer.h"
#include "objects/Object.h"
#include "objects/wrappers/Flags.h"
#include "run/VirtualSettings.h"
#include <wx/window.h>

class wxDC;
class wxPaintDC;

NAMESPACE_SPH_BEGIN

class Path;
class IRenderContext;
class Statistics;

using FileFormat = IVirtualEntry::FileFormat;

Optional<Path> doSaveFileDialog(const String& title, Array<FileFormat>&& formats);

Optional<Path> doOpenFileDialog(const String& title, Array<FileFormat>&& formats);

int messageBox(const String& message, const String& title, const int buttons);

/// Draws a text into the DC using current font and color, replacing all _x with corresponding subscript
/// character.
void drawTextWithSubscripts(wxDC& dc, const String& text, const wxPoint point);

/// Converts the value to a printable string.
String toPrintableString(const Float value, const Size precision = 5, const Float decimalThreshold = 1000._f);

void printLabels(wxDC& dc, ArrayView<const IRenderOutput::Label> labels);

void drawKey(IRenderContext& context, const Statistics& stats, const float wtp, const Rgba& background);

void drawAxis(IRenderContext& context, const Rgba& color, const Vector& axis, const String& label);

class BusyCursor {
private:
    wxWindow* window;

public:
    explicit BusyCursor(wxWindow* window)
        : window(window) {
        window->SetCursor(*wxHOURGLASS_CURSOR);
        wxYield();
    }

    ~BusyCursor() {
        window->SetCursor(*wxSTANDARD_CURSOR);
    }
};

class TransparencyPattern {
private:
    Bitmap<Rgba> stipple;

public:
    explicit TransparencyPattern(const Size side = 8,
        const Rgba& dark = Rgba::gray(0.25f),
        const Rgba& light = Rgba::gray(0.3f));

    void draw(wxDC& dc, const wxRect& rect) const;
};

NAMESPACE_SPH_END
