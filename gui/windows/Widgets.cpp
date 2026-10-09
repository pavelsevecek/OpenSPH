#include "gui/windows/Widgets.h"
#include "gui/Theme.h"
#include "gui/MainLoop.h"
#include "gui/Utils.h"
#include "thread/CheckFunction.h"
#include <wx/msgdlg.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/props.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

NAMESPACE_SPH_BEGIN

FloatTextCtrl::FloatTextCtrl(wxWindow* parent, const double value, const Interval range)
    : value(value)
    , range(range) {

    lastValidValue = value;

    wxValidator* validator = wxFloatProperty::GetClassValidator();
    this->Create(parent,
        wxID_ANY,
        std::to_string(value),
        wxDefaultPosition,
        wxSize(100, 25),
        wxTE_PROCESS_ENTER | wxTE_RIGHT,
        *validator);

    this->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent& evt) {
        this->validate();
        evt.Skip();
    });
    this->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& evt) {
        this->validate();
        evt.Skip();
    });

    this->validate();
}

void FloatTextCtrl::setValue(double newValue) {
    value = range.clamp(newValue);
    lastValidValue = value;

    wxFloatProperty prop;
    wxVariant variant(value);
    this->ChangeValue(prop.ValueToString(variant));
}

#ifdef __WXMSW__
WXLRESULT FloatTextCtrl::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) {
    const WXLRESULT result = wxTextCtrl::MSWWindowProc(message, wParam, lParam);
    if (message == WM_NCPAINT || message == WM_PAINT || message == WM_SETFOCUS ||
        message == WM_KILLFOCUS || message == WM_ENABLE) {
        // Replace the native bright bevel, without changing the edit area's
        // size or painting over its text, selection or caret.
        HWND handle = static_cast<HWND>(this->GetHandle());
        RECT bounds;
        RECT client;
        if (!handle || !::GetWindowRect(handle, &bounds) || !::GetClientRect(handle, &client)) {
            return result;
        }
        ::MapWindowPoints(handle, nullptr, reinterpret_cast<POINT*>(&client), 2);
        ::OffsetRect(&client, -bounds.left, -bounds.top);
        ::OffsetRect(&bounds, -bounds.left, -bounds.top);
        HDC dc = ::GetWindowDC(handle);
        if (dc) {
            const int saved = ::SaveDC(dc);
            ::ExcludeClipRect(dc, client.left, client.top, client.right, client.bottom);
            const COLORREF colour = !this->IsEnabled() ? RGB(48, 51, 57)
                                    : ::GetFocus() == handle ? RGB(88, 96, 108)
                                                           : RGB(65, 69, 77);
            HBRUSH brush = ::CreateSolidBrush(colour);
            ::FillRect(dc, &bounds, brush);
            ::DeleteObject(brush);
            ::RestoreDC(dc, saved);
            ::ReleaseDC(handle, dc);
        }
    }
    return result;
}

WXHBRUSH FloatTextCtrl::MSWControlColor(WXHDC dc, WXHWND window) {
    // wxTextCtrl substitutes COLOR_BTNFACE for disabled single-line controls.
    // Keep the native disabled behavior, but use our own background brush.
    WXHBRUSH brush = DoMSWControlColor(dc, DarkTheme::background(), window);
    if (!this->IsEnabled()) {
        ::SetTextColor(static_cast<HDC>(dc), RGB(145, 149, 157));
    }
    return brush;
}
#endif

void FloatTextCtrl::validate() {
    wxFloatProperty prop;
    wxVariant variant = this->GetValue();
    wxPGValidationInfo info;
    if (!prop.ValidateValue(variant, info)) {
        value = lastValidValue;
    } else {
        value = float(variant.GetDouble());
    }

    value = range.clamp(value);

    if (onValueChanged && value != lastValidValue) {
        const bool validated = onValueChanged(value);
        if (!validated) {
            value = lastValidValue;
        }
    }

    lastValidValue = value;
    variant = double(value);
    this->ChangeValue(prop.ValueToString(variant));
}

class WaitDialog : public wxDialog {
public:
    WaitDialog(wxWindow* parent, const String& message)
        : wxDialog(parent, wxID_ANY, "Info", wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU) {
        const wxSize size = wxSize(320, 90);
        this->SetSize(size);
        wxStaticText* text = new wxStaticText(this, wxID_ANY, message.toUnicode());
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->AddStretchSpacer();
        sizer->Add(text, 1, wxALIGN_CENTER_HORIZONTAL);
        sizer->AddStretchSpacer();
        this->SetSizer(sizer);
        this->Layout();
        this->CentreOnScreen();
    }
};

ClosablePage::ClosablePage(wxWindow* parent, const String& label)
    : wxPanel(parent, wxID_ANY)
    , label(label) {}

bool ClosablePage::close() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
    if (this->isRunning()) {
        const int retval = messageBox(
            capitalize(label) + " is currently in progress. Do you want to stop it and close the window?",
            "Stop?",
            wxYES_NO | wxCENTRE);
        if (retval == wxYES) {
            this->stop();
            dialog = new WaitDialog(this, "Waiting for " + label + " to finish ...");
            dialog->ShowModal();
            this->quit();
            return true;
        } else {
            return false;
        }
    } else {
        return true;
    }
}

void ClosablePage::onStopped() {
    if (dialog) {
        executeOnMainThread([weakDialog = wxWeakRef<WaitDialog>(dialog)] {
            if (weakDialog) {
                weakDialog->EndModal(0);
            }
        });
    }
}


NAMESPACE_SPH_END
