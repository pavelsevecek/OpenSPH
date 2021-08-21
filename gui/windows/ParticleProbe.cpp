#include "gui/windows/ParticleProbe.h"
#include "quantities/QuantityIds.h"

NAMESPACE_SPH_BEGIN

inline String toName(const String& s) {
    String name = s;
    name.replaceAll("_", " ");
    return capitalize(name);
}

inline String toKey(const String& s) {
    return split(s, '.').back();
}

void ParticleProbe::onMenu(wxCommandEvent& UNUSED(evt)) {
    if (!particle) {
        return;
    }

    if (!wxTheClipboard->Open()) {
        return;
    }
    wxTheClipboard->Clear();

    for (Particle::QuantityData data : particle->getQuantities()) {
        std::stringstream ss;
        if (data.id == QuantityId::POSITION) {
            if (!data.dt.empty()) {
                ss << data.dt;
                wxTheClipboard->SetData(new wxTextDataObject(ss.str()));
            }
            continue;
        }
        if (!data.value.empty()) {
            std::stringstream ss;
            ss << data.value;
            wxTheClipboard->SetData(new wxTextDataObject(ss.str()));
        }
    }

    wxTheClipboard->Flush();
    wxTheClipboard->Close();
}

void ParticleProbe::onPaint(wxPaintEvent& UNUSED(evt)) {
    wxAutoBufferedPaintDC dc(this);
    wxSize canvasSize = this->GetClientSize();

    // draw background
    Rgba backgroundColor = Rgba(this->GetParent()->GetBackgroundColour());
    wxBrush brush;
    brush.SetColour(wxColour(backgroundColor.darken(0.3f)));
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
    const bool isLightTheme = backgroundColor.intensity() > 0.5_f;
    if (isLightTheme) {
        dc.SetTextForeground(wxColour(Rgba(0.2f, 0.2f, 0.2f)));
    } else {
        dc.SetTextForeground(wxColour(Rgba(0.8f, 0.8f, 0.8f)));
    }
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
    for (Particle::QuantityData data : particle->getQuantities()) {
        if (data.id == QuantityId::POSITION) {
            // skip position info, already printed
            continue;
        }

        /// \todo Currently the only displayed derivatvies are velocities and they are already printed, so
        /// although we can assume there are only quantity values, generalize it for future uses.

        SPH_ASSERT(!data.value.empty());
        const DynamicId id = data.value.getType();
        const String label = getMetadata(data.id).label;
        switch (id) {
        case DynamicId::FLOAT:
            /// \todo we should use the name of the colorizer, not quantity
            /// \todo replace full quantity name with shorter designation (rho, ...)
            drawTextWithSubscripts(dc, label + L" = " + toPrintableString(data.value.get<Float>()), offset);
            offset.y += config.lineSkip;
            break;
        case DynamicId::SIZE:
            drawTextWithSubscripts(dc, label + L" = " + toString(data.value.get<Size>()), offset);
            offset.y += config.lineSkip;
            break;
        case DynamicId::VECTOR: {
            const Vector vector = data.value;
            this->printVector(dc, vector, label, offset);
            offset.y += 3 * config.lineSkip;
            break;
        }
        case DynamicId::TRACELESS_TENSOR: {
            const TracelessTensor tensor = data.value;
            this->printTensor(dc, tensor, label, offset);
            offset.y += 6 * config.lineSkip;
            break;
        }
        case DynamicId::SYMMETRIC_TENSOR: {
            const SymmetricTensor tensor = data.value;
            this->printTensor(dc, tensor, label, offset);
            offset.y += 6 * config.lineSkip;
            break;
        }

        default:
            // don't throw, but don't print anything
            break;
        }
    }

    for (const Particle::ParamData& data : particle->getParameters()) {
        String label = BodySettings::getEntryName(data.id).value();
        const DynamicId id = data.value.getType();
        switch (id) {
        case DynamicId::STRING:
            drawTextWithSubscripts(dc, toKey(label) + " = " + toName(data.value.get<String>()), offset);
            offset.y += config.lineSkip;
            break;
        default:
            break;
        }
    }
}

void ParticleProbe::printVector(wxDC& dc, const Vector& v, const String& label, const wxPoint offset) const {
    drawTextWithSubscripts(
        dc, label + L"_x = " + toPrintableString(v[X]), offset + wxSize(0, 0 * config.lineSkip));
    drawTextWithSubscripts(
        dc, label + L"_y = " + toPrintableString(v[Y]), offset + wxSize(0, 1 * config.lineSkip));
    drawTextWithSubscripts(
        dc, label + L"_z = " + toPrintableString(v[Z]), offset + wxSize(0, 2 * config.lineSkip));
}

template <typename Type>
void ParticleProbe::printTensor(wxDC& dc,
    const Type& tensor,
    const String& label,
    const wxPoint offset) const {
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
        dc, label + L"_yz = " + toPrintableString(tensor(Y, Z)), offset + wxSize(x, 2 * config.lineSkip));
}

NAMESPACE_SPH_END
