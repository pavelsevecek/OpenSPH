#pragma once

/// \file ParticleProbe.h
/// \brief Frame showing information about selected particle
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "gui/Utils.h"
#include "gui/objects/Color.h"
#include "quantities/Particle.h"
#include <wx/clipbrd.h>
#include <wx/dcclient.h>
#include <wx/menu.h>
#include <wx/panel.h>
#include <wx/textctrl.h>

NAMESPACE_SPH_BEGIN

class ParticleProbe : public wxPanel {
private:
    /// Currently selected particle (if there is one)
    Optional<Particle> particle;

    /// Color used to draw the particle by the renderer
    Rgba color;

    struct {

        /// Padding on top
        Size topSkip = 5;

        /// Padding on left end
        Size leftSkip = 5;

        /// Height of a line of text
        Size lineSkip = 19;

    } config;

public:
    ParticleProbe(wxWindow* parent, const wxSize size)
        : wxPanel(parent, wxID_ANY) {
        this->SetMinSize(size);

        // Connect handlers
        Connect(wxEVT_PAINT, wxPaintEventHandler(ParticleProbe::onPaint));
        Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(ParticleProbe::onRightUp));
    }

    void update(const Particle& selectedParticle, const Rgba colorizerColor) {
        particle = selectedParticle;
        color = colorizerColor;
        this->Refresh();
    }

    void clear() {
        particle = NOTHING;
        this->Refresh();
    }

private:
    void onRightUp(wxMouseEvent& UNUSED(evt)) {
        wxMenu menu;
        menu.Append(0, "Copy to clipboard");
        menu.Connect(
            wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(ParticleProbe::onMenu), nullptr, this);
        this->PopupMenu(&menu);
    }

    void onMenu(wxCommandEvent& evt);

    void onPaint(wxPaintEvent& evt);

    void printVector(wxDC& dc, const Vector& v, const std::wstring& label, const wxPoint offset) const;

    template <typename Type>
    void printTensor(wxDC& dc, const Type& tensor, const std::wstring& label, const wxPoint offset) const;
};

NAMESPACE_SPH_END
