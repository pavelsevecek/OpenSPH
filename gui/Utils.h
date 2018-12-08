#pragma once

/// \file Utils.h
/// \brief Random utility functions for drawing stuff to DC
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/Globals.h"
#include "gui/renderers/IRenderer.h"
#include "objects/Object.h"
#include "objects/wrappers/Flags.h"
#include <wx/dc.h>

NAMESPACE_SPH_BEGIN

/// Draws a text into the DC using current font and color, replacing all _x with corresponding subscript
/// character.
void drawTextWithSubscripts(wxDC& dc, const std::wstring& text, const wxPoint point);

/// Converts the value to a printable string.
std::wstring toPrintableString(const float value,
    const Size precision = 5,
    const float decimalThreshold = 1000.f);

void printLabels(wxDC& dc, ArrayView<const IRenderOutput::Label> labels);

NAMESPACE_SPH_END
