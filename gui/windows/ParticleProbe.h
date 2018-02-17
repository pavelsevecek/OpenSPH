#pragma once

/// \file ParticleProbe.h
/// \brief Frame showing information about selected particle
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gui/Utils.h"
#include "gui/objects/Color.h"
#include "quantities/Particle.h"
#include <wx/dcclient.h>
#include <wx/panel.h>
#include <wx/textctrl.h>

NAMESPACE_SPH_BEGIN

class ParticleProbe : public wxPanel {
private:
    /// Currently selected particle (if there is one)
    Optional<Particle> particle;

    /// Color used to draw the particle by the renderer
    Color color;

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
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, size) {

        this->SetMinSize(size);
        this->SetMaxSize(size);

        // Connect handlers
        Connect(wxEVT_PAINT, wxPaintEventHandler(ParticleProbe::onPaint));
    }

    void update(const Particle& selectedParticle, const Color colorizerColor) {
        particle = selectedParticle;
        color = colorizerColor;
        this->Refresh();
    }

    void clear() {
        particle = NOTHING;
        this->Refresh();
    }

private:
    void onPaint(wxPaintEvent& UNUSED(evt)) {
        wxPaintDC dc(this);
        wxSize canvasSize = dc.GetSize();

        // draw background
        Color backgroundColor = Color(this->GetParent()->GetBackgroundColour());
        wxBrush brush;
        brush.SetColour(wxColour(backgroundColor.darken(0.2_f)));
        dc.SetBrush(brush);
        dc.DrawRectangle(wxPoint(0, 0), canvasSize);

        if (!particle) {
            return;
        }

        // draw colored square
        brush.SetColour(wxColour(color));
        dc.SetBrush(brush);
        wxPoint offset(config.leftSkip, config.topSkip);
        dc.DrawRectangle(offset, wxSize(15, 15));

        // particle index
        dc.SetTextForeground(wxColour(Color(0.8f, 0.8f, 0.8f)));
        dc.DrawText("Particle " + std::to_string(particle->getIndex()), wxPoint(24, 4));

        // particle position
        const Vector position = particle->getValue(QuantityId::POSITION);
        drawTextWithSubscripts(
            dc, L"x = " + toPrintableString(position[X]), offset + wxSize(0, 1 * config.lineSkip));
        drawTextWithSubscripts(
            dc, L"y = " + toPrintableString(position[Y]), offset + wxSize(0, 2 * config.lineSkip));
        drawTextWithSubscripts(
            dc, L"z = " + toPrintableString(position[Z]), offset + wxSize(0, 3 * config.lineSkip));

        Dynamic velocityValue = particle->getDt(QuantityId::POSITION);
        Dynamic accelerationValue = particle->getD2t(QuantityId::POSITION);
        if (velocityValue) {
            const Vector velocity = velocityValue;
            const Size x = canvasSize.x / 2;
            drawTextWithSubscripts(
                dc, L"v_x = " + toPrintableString(velocity[X]), offset + wxSize(x, 1 * config.lineSkip));
            drawTextWithSubscripts(
                dc, L"v_y = " + toPrintableString(velocity[Y]), offset + wxSize(x, 2 * config.lineSkip));
            drawTextWithSubscripts(
                dc, L"v_z = " + toPrintableString(velocity[Z]), offset + wxSize(x, 3 * config.lineSkip));
        } else if (accelerationValue) {
            const Vector acceleration = accelerationValue;
            const Size x = canvasSize.x / 2;
            drawTextWithSubscripts(
                dc, L"dv_x = " + toPrintableString(acceleration[X]), offset + wxSize(x, 1 * config.lineSkip));
            drawTextWithSubscripts(
                dc, L"dv_y = " + toPrintableString(acceleration[Y]), offset + wxSize(x, 2 * config.lineSkip));
            drawTextWithSubscripts(
                dc, L"dv_z = " + toPrintableString(acceleration[Z]), offset + wxSize(x, 3 * config.lineSkip));
        }

        offset.y += 4 * config.lineSkip;

        // print other particle data
        for (Particle::QuantityData data : particle.value()) {
            if (data.id == QuantityId::POSITION) {
                // skip position info, already printed
                continue;
            }

            /// \todo Currently the only displayed derivatvies are velocities and they are already printed, so
            /// although we can assume there are only quantity values, generalize it for future uses.

            ASSERT(!data.value.empty());
            const DynamicId id = data.value.getType();
            const std::wstring label = getMetadata(data.id).label;
            switch (id) {
            case DynamicId::FLOAT:
                /// \todo we should use the name of the colorizer, not quantity
                /// \todo replace full quantity name with shorter designation (rho, ...)
                drawTextWithSubscripts(
                    dc, label + L" = " + toPrintableString(data.value.get<Float>()), offset);
                offset.y += config.leftSkip;
                break;
            case DynamicId::VECTOR: {
                const Vector vector = data.value;
                this->printVector(dc, vector, label, offset);
                offset.y += 3 * config.leftSkip;
                break;
            }
            case DynamicId::TRACELESS_TENSOR: {
                const TracelessTensor tensor = data.value;
                this->printTensor(dc, tensor, label, offset);
                offset.y += 6 * config.leftSkip;
                break;
            }
            case DynamicId::SYMMETRIC_TENSOR: {
                const SymmetricTensor tensor = data.value;
                this->printTensor(dc, tensor, label, offset);
                offset.y += 6 * config.leftSkip;
                break;
            }

            default:
                // don't throw, but don't print anything
                break;
            }
        }
    }

    void printVector(wxDC& dc, const Vector& v, const std::wstring& label, const wxPoint offset) const {
        drawTextWithSubscripts(
            dc, label + L"_x = " + toPrintableString(v[X]), offset + wxSize(0, 0 * config.lineSkip));
        drawTextWithSubscripts(
            dc, label + L"_y = " + toPrintableString(v[Y]), offset + wxSize(0, 1 * config.lineSkip));
        drawTextWithSubscripts(
            dc, label + L"_z = " + toPrintableString(v[Z]), offset + wxSize(0, 2 * config.lineSkip));
    }

    template <typename Type>
    void printTensor(wxDC& dc, const Type& tensor, const std::wstring& label, const wxPoint offset) const {
        const Size x = dc.GetSize().x / 2;
        drawTextWithSubscripts(
            dc, label + L"_xx = " + toPrintableString(tensor(X, X)), offset + wxSize(0, 0 * config.lineSkip));
        drawTextWithSubscripts(
            dc, label + L"_yy = " + toPrintableString(tensor(Y, Y)), offset + wxSize(0, 1 * config.lineSkip));
        drawTextWithSubscripts(
            dc, label + L"_zz = " + toPrintableString(tensor(Z, Z)), offset + wxSize(0, 2 * config.lineSkip));
        drawTextWithSubscripts(
            dc, label + L"_xy = " + toPrintableString(tensor(X, Y)), offset + wxSize(x, 0 * config.lineSkip));
        drawTextWithSubscripts(
            dc, label + L"_xz = " + toPrintableString(tensor(X, Z)), offset + wxSize(x, 1 * config.lineSkip));
        drawTextWithSubscripts(
            dc, label + L"_yz = " + toPrintableString(tensor(Y, X)), offset + wxSize(x, 2 * config.lineSkip));
    }
};

NAMESPACE_SPH_END
