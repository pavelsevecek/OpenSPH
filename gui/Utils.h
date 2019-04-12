#pragma once

/// \file Utils.h
/// \brief Random utility functions for drawing stuff to DC
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/Globals.h"
#include "gui/renderers/IRenderer.h"
#include "objects/Object.h"
#include "objects/wrappers/Flags.h"

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

/// Converts the value to a printable string.
std::wstring toPrintableString(const float value,
    const Size precision = 5,
    const float decimalThreshold = 1000.f);

void printLabels(wxDC& dc, ArrayView<const IRenderOutput::Label> labels);

NAMESPACE_SPH_END
