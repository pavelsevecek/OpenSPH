#pragma once

#include "objects/Object.h"
#include <wx/dc.h>

NAMESPACE_SPH_BEGIN

/// Draws a text into the DC using current font and color, replacing all _x with corresponding subscript
/// character.
void drawTextWithSubscripts(wxDC& dc, const std::string& text, const wxPoint point);

NAMESPACE_SPH_END
