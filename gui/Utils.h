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

using FileFormat = IVirtualEntry::FileFormat;

Optional<Path> doSaveFileDialog(const std::string& title, Array<FileFormat>&& formats);

Optional<Path> doOpenFileDialog(const std::string& title, Array<FileFormat>&& formats);

/// Draws a text into the DC using current font and color, replacing all _x with corresponding subscript
/// character.
void drawTextWithSubscripts(wxDC& dc, const std::wstring& text, const wxPoint point);

void drawTextWithSubscripts(wxDC& dc, const std::string& text, const wxPoint point);

/// Converts the value to a printable string.
std::wstring toPrintableString(const Float value,
    const Size precision = 5,
    const Float decimalThreshold = 1000._f);

void printLabels(wxDC& dc, ArrayView<const IRenderOutput::Label> labels);

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
