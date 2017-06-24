#include "gui/Utils.h"

NAMESPACE_SPH_BEGIN

void drawTextWithSubscripts(wxDC& dc, const std::string& text, const wxPoint point) {
    std::size_t n;
    std::size_t m = 0;
    wxPoint actPoint = point;
    const wxFont font = dc.GetFont();
    const wxFont subcriptFont = font.Smaller();

    while ((n = text.find('_', m)) != std::string::npos) {
        std::string part = text.substr(m, n - m);
        wxSize extent = dc.GetTextExtent(part);
        // draw part up to subscript using current font
        dc.DrawText(part, actPoint);

        actPoint.x += extent.x;
        const char subscript = text[n + 1];
        dc.SetFont(subcriptFont);
        wxPoint subscriptPoint(actPoint.x + 2, actPoint.y + extent.y / 2);
        dc.DrawText(wxString(subscript), subscriptPoint);
        actPoint.x = subscriptPoint.x + dc.GetTextExtent(wxString(subscript)).x;

        dc.SetFont(font);
        m = n + 2; // skip _ and the subscript character
    }
    // draw last part of the text
    dc.DrawText(text.substr(m), actPoint);
}


NAMESPACE_SPH_END
