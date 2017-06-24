#pragma once

#include "common/Globals.h"
#include "objects/Object.h"
#include <wx/dc.h>

NAMESPACE_SPH_BEGIN

/// Draws a text into the DC using current font and color, replacing all _x with corresponding subscript
/// character.
void drawTextWithSubscripts(wxDC& dc, const std::wstring& text, const wxPoint point);

/// Converts the value to a printable string.
std::wstring toPrintableString(const float value,
    const Size precision = 5,
    const float decimalThreshold = 1000.f);

NAMESPACE_SPH_END
