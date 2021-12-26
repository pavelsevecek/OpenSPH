#pragma once

#include "objects/containers/String.h"
#include <wx/popupwin.h>

NAMESPACE_SPH_BEGIN

class Tooltip : public wxPopupWindow {
public:
    Tooltip(wxWindow* parent, wxPoint position, const String& text);
};

template <typename TWindow, typename TId = int>
class TooltippedWindow : public TWindow {
private:
    Tooltip* activeTooltip = nullptr;
    wxRect activeRect;
    TId activeId;

public:
    using TWindow::TWindow;

    void showTooltip(wxPoint position, const wxRect& rect, TId id, const String& text) {
        if (activeTooltip) {
            if (id == activeId) {
                // the same tooltip, skip
                return;
            }

            activeTooltip->Destroy();
        }

        Tooltip* tip = new Tooltip(this, position, text);
        activeRect = rect;
        tip->Show();
        activeTooltip = tip;
        activeId = id;
    }

    void checkTooltips(wxPoint position) {
        if (!activeTooltip) {
            return;
        }

        if (!activeRect.Contains(position)) {
            activeTooltip->Destroy();
            activeTooltip = nullptr;
        }
    }
};

NAMESPACE_SPH_END
