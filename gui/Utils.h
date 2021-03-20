#pragma once

/// \file Utils.h
/// \brief Random utility functions for drawing stuff to DC
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/Globals.h"
#include "gui/renderers/IRenderer.h"
#include "objects/Object.h"
#include "objects/wrappers/Flags.h"
#include <wx/window.h>

class wxDC;

NAMESPACE_SPH_BEGIN

class Path;

struct FileFormat {
    std::string desc;
    std::string ext;
};

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
    BusyCursor(wxWindow* window)
        : window(window) {
        window->SetCursor(*wxHOURGLASS_CURSOR);
        wxYield();
    }

    ~BusyCursor() {
        window->SetCursor(*wxSTANDARD_CURSOR);
    }
};

NAMESPACE_SPH_END
