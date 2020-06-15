#include "gui/windows/GridPage.h"
#include "gui/MainLoop.h"
#include "gui/Utils.h"
#include "io/Path.h"
#include "quantities/Quantity.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"
#include "windows/Widgets.h"
#include <fstream>
#include <numeric>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

NAMESPACE_SPH_BEGIN

/// Flags used as window ID and as parameters to compute
///
/// \note When adding a new parameter, you have to:
/// 1. Increment constant CHECK_COUNT
/// 2. Add the parameter to the list in \ref getCheckedCount
/// 3. Make sure the evaluating function in \ref updateAsync is at the right position, it has to match the
/// position of the parameter within \ref CheckFlag.
enum class CheckFlag {
    PARTICLE_COUNT = 1 << 0,
    MASS_FRACTION = 1 << 1,
    DIAMETER = 1 << 2,
    VELOCITY_DIFFERENCE = 1 << 3,
    PERIOD = 1 << 4,
    RATIO_CB = 1 << 5,
    RATIO_BA = 1 << 6,
    SPHERICITY = 1 << 7,
    MOONS = 1 << 8,
};
const Size CHECK_COUNT = 9;

GridPage::GridPage(wxWindow* parent, const wxSize size, const Storage& storage)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
    , storage(storage) {

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* countSizer = new wxBoxSizer(wxHORIZONTAL);
    countSizer->Add(new wxStaticText(this, wxID_ANY, "Number of fragments"));
    countSpinner = new wxSpinCtrl(this, wxID_ANY);
    countSpinner->SetValue(4);
    countSizer->Add(countSpinner);
    sizer->Add(countSizer);

    wxGridSizer* boxSizer = new wxGridSizer(4, 2, 2);
    boxSizer->Add(new wxCheckBox(this, int(CheckFlag::PARTICLE_COUNT), "Particle count"));
    boxSizer->Add(new wxCheckBox(this, int(CheckFlag::MASS_FRACTION), "Mass fraction"));
    boxSizer->Add(new wxCheckBox(this, int(CheckFlag::DIAMETER), "Diameter [km]"));
    boxSizer->Add(new wxCheckBox(this, int(CheckFlag::VELOCITY_DIFFERENCE), "Velocity difference [m/s]"));
    boxSizer->Add(new wxCheckBox(this, int(CheckFlag::PERIOD), "Period [h]"));
    boxSizer->Add(new wxCheckBox(this, int(CheckFlag::RATIO_CB), "Ratio c/b"));
    boxSizer->Add(new wxCheckBox(this, int(CheckFlag::RATIO_BA), "Ratio b/a"));
    boxSizer->Add(new wxCheckBox(this, int(CheckFlag::SPHERICITY), "Sphericity"));
    sizer->Add(boxSizer);

    wxStaticBox* moonGroup = new wxStaticBox(this, wxID_ANY, "Moons", wxDefaultPosition, wxSize(-1, 60));
    wxBoxSizer* moonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxCheckBox* moonBox = new wxCheckBox(moonGroup, int(CheckFlag::MOONS), "Moon counts");
    moonSizer->Add(moonBox);
    moonSizer->AddSpacer(30);
    moonSizer->Add(new wxStaticText(moonGroup, wxID_ANY, "Mass ratio limit"));
    FloatTextCtrl* limitSpinner = new FloatTextCtrl(moonGroup, 0.1f);
    moonSizer->Add(limitSpinner);
    limitSpinner->Enable(false);
    moonGroup->SetSizer(moonSizer);
    moonSizer->AddSpacer(30);
    moonSizer->Add(new wxStaticText(moonGroup, wxID_ANY, "Pericenter limit"));
    FloatTextCtrl* radiiSpinner = new FloatTextCtrl(moonGroup, 2.f);
    moonSizer->Add(radiiSpinner);
    radiiSpinner->Enable(false);
    moonGroup->SetSizer(moonSizer);
    sizer->Add(moonGroup);
    moonBox->Bind(wxEVT_CHECKBOX, [moonBox, limitSpinner, radiiSpinner](wxCommandEvent&) {
        limitSpinner->Enable(moonBox->GetValue());
        radiiSpinner->Enable(moonBox->GetValue());
    });

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* computeButton = new wxButton(this, wxID_ANY, "Compute");
    buttonSizer->Add(computeButton);

    wxButton* saveButton = new wxButton(this, wxID_ANY, "Save to file");
    saveButton->Enable(false);
    buttonSizer->Add(saveButton);
    sizer->Add(buttonSizer);

    this->SetSizer(sizer);
    this->Layout();

    computeButton->Bind(wxEVT_BUTTON,
        [this, size, sizer, saveButton, limitSpinner, radiiSpinner](wxCommandEvent& UNUSED(evt)) {
            const Size checkCount = this->getCheckedCount();
            if (checkCount == 0) {
                wxMessageBox("No parameters selected", "Fail", wxOK | wxCENTRE);
                return;
            }
            if (grid != nullptr) {
                sizer->Detach(grid);
                this->RemoveChild(grid);
            }
            grid = new wxGrid(this, wxID_ANY, wxDefaultPosition, size);
            grid->EnableEditing(false);
            sizer->Add(grid);
            grid->CreateGrid(countSpinner->GetValue(), checkCount);
            this->Layout();
            saveButton->Enable(true);

            Config config;
            config.moonLimit = limitSpinner->getValue();
            config.radiiLimit = radiiSpinner->getValue();
            this->update(this->storage, config);
        });

    saveButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) {
        Optional<Path> path = doSaveFileDialog("Save to file", { { "Text file", "txt" } });
        if (path) {
            std::ofstream ofs(path->native());
            ofs << "#";
            for (int col = 0; col < grid->GetNumberCols(); ++col) {
                const wxString label = grid->GetColLabelValue(col);
                ofs << std::string(max(1, 26 - int(label.size())), ' ') << label;
            }
            ofs << "\n";
            for (int row = 0; row < grid->GetNumberRows(); ++row) {
                ofs << "  ";
                for (int col = 0; col < grid->GetNumberCols(); ++col) {
                    ofs << std::setw(25) << grid->GetCellValue(row, col) << " ";
                }
                ofs << "\n";
            }
        }
    });
}

GridPage::~GridPage() {
    if (thread.joinable()) {
        thread.join();
    }
}

class ComponentGetter {
private:
    const Storage& storage;
    Array<Size> indices;
    Size maxIdx;

    mutable struct {
        Array<Float> masses;
        Array<Vector> positions;
        Array<Vector> velocities;
    } lazy;

public:
    explicit ComponentGetter(const Storage& storage)
        : storage(storage) {
        const Flags<Post::ComponentFlag> flags =
            Post::ComponentFlag::OVERLAP | Post::ComponentFlag::SORT_BY_MASS;
        maxIdx = Post::findComponents(storage, 2._f, flags, indices);
    }

    Storage getComponent(const Size idx) {
        Storage component = storage.clone(VisitorEnum::ALL_BUFFERS);
        Array<Size> toRemove;
        for (Size i = 0; i < indices.size(); ++i) {
            if (indices[i] != idx) {
                toRemove.push(i);
            }
        }
        component.remove(toRemove, Storage::IndicesFlag::INDICES_SORTED);
        return component;
    }

    ArrayView<const Float> getMasses() const {
        if (lazy.masses.empty()) {
            this->computeIntegrals();
        }
        return lazy.masses;
    }

    ArrayView<const Vector> getPositions() const {
        if (lazy.positions.empty()) {
            this->computeIntegrals();
        }
        return lazy.positions;
    }

    ArrayView<const Vector> getVelocities() const {
        if (lazy.velocities.empty()) {
            this->computeIntegrals();
        }
        return lazy.velocities;
    }

private:
    void computeIntegrals() const {
        lazy.masses.resizeAndSet(maxIdx, 0._f);
        lazy.positions.resizeAndSet(maxIdx, Vector(0._f));
        lazy.velocities.resizeAndSet(maxIdx, Vector(0._f));

        Array<Float> radii(maxIdx);
        radii.fill(0._f);

        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        for (Size i = 0; i < indices.size(); ++i) {
            lazy.masses[indices[i]] += m[i];
            lazy.positions[indices[i]] += m[i] * r[i];
            lazy.velocities[indices[i]] += m[i] * v[i];
            radii[indices[i]] += pow<3>(r[i][H]);
        }
        for (Size k = 0; k < maxIdx; ++k) {
            lazy.positions[k] /= lazy.masses[k];
            lazy.positions[k][H] = cbrt(radii[k]);
            lazy.velocities[k] /= lazy.masses[k];
        }
    }
};

static Pair<Float> getMassAndDiameter(const Storage& storage) {
    if (!storage.has(QuantityId::DENSITY)) {
        return { 0._f, 0._f }; // some fallback?
    }
    ArrayView<const Float> m, rho;
    tie(m, rho) = storage.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY);

    Float mass = 0._f;
    Float diameter = 0._f;
    for (Size i = 0; i < m.size(); ++i) {
        mass += m[i];
        diameter += m[i] / rho[i];
    }
    diameter = cbrt(3._f * diameter / (4._f * PI)) * 2._f;
    return { mass, diameter };
}

static Pair<Float> getSemiaxisRatios(const Storage& storage) {
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    const SymmetricTensor I = Post::getInertiaTensor(m, r);
    const Eigen e = eigenDecomposition(I);
    const Float A = e.values[0];
    const Float B = e.values[1];
    const Float C = e.values[2];
    const Float a = sqrt(B + C - A);
    const Float b = sqrt(A + C - B);
    const Float c = sqrt(A + B - C);
    ASSERT(a > 0._f && b > 0._f && c > 0._f, a, b, c);
    return { c / b, b / a };
}

static Float getSphericity(const Storage& storage) {
    SharedPtr<IScheduler> scheduler = Factory::getScheduler(RunSettings::getDefaults());
    return Post::getSphericity(*scheduler, storage, 0.02_f);
}

static Float getVelocityDifference(const Storage& s1, const Storage& s2) {
    ArrayView<const Float> m1 = s1.getValue<Float>(QuantityId::MASS);
    ArrayView<const Vector> v1 = s1.getDt<Vector>(QuantityId::POSITION);

    Float mtot1 = 0._f;
    Vector p1(0._f);
    for (Size i = 0; i < m1.size(); ++i) {
        mtot1 += m1[i];
        p1 += m1[i] * v1[i];
    }

    ArrayView<const Float> m2 = s2.getValue<Float>(QuantityId::MASS);
    ArrayView<const Vector> v2 = s2.getDt<Vector>(QuantityId::POSITION);

    Float mtot2 = 0._f;
    Vector p2(0._f);
    for (Size i = 0; i < m2.size(); ++i) {
        mtot2 += m2[i];
        p2 += m2[i] * v2[i];
    }

    return getLength(p1 / mtot1 - p2 / mtot2);
}

static Float getPeriod(const Storage& storage) {
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    const Float omega = getLength(Post::getAngularFrequency(m, r, v));
    if (omega == 0._f) {
        return 0._f;
    } else {
        return 2._f * PI / (3600._f * omega);
    }
}

static Size getMoons(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    ArrayView<const Vector> v,
    const Size idx,
    const Float limit,
    const Float radius) {
    return Post::findMoonCount(m, r, v, idx, radius, limit);
}

void GridPage::update(const Storage& storage, const Config& config) {
    if (thread.joinable()) {
        wxMessageBox("Computation in progress", "Fail", wxOK | wxCENTRE);
        return;
    }
    Size colIdx = 0;
    Flags<CheckFlag> checks;
    for (Size i = 0; i < CHECK_COUNT; ++i) {
        const CheckFlag flag = CheckFlag(1 << i);
        wxCheckBox* box = this->getCheck(flag);
        if (box->GetValue()) {
            grid->SetColLabelValue(colIdx++, box->GetLabel());
            checks.set(flag);
        }
    }
    grid->AutoSize();

    const Size fragmentCnt = countSpinner->GetValue();

    thread = std::thread([this, &storage, fragmentCnt, checks, config] {
        try {
            this->updateAsync(storage, fragmentCnt, checks, config);
        } catch (std::exception& e) {
            executeOnMainThread([message = std::string(e.what())] {
                wxMessageBox("Failed to compute fragment parameters.\n\n" + message, "Fail", wxOK | wxCENTRE);
            });
        }
    });
}

void GridPage::updateAsync(const Storage& storage,
    const Size fragmentCnt,
    const Flags<CheckFlag> checks,
    const Config& config) {
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    const Float totalMass = std::accumulate(m.begin(), m.end(), 0._f);

    ComponentGetter getter(storage);

    Storage lr;
    for (Size i = 0; i < fragmentCnt; ++i) {
        const Storage fragment = getter.getComponent(i);
        if (fragment.getParticleCnt() < 2) {
            break;
        }
        if (i == 0) {
            lr = fragment.clone(VisitorEnum::ALL_BUFFERS);
        }

        // order must match the values of CheckFlag
        Size colIdx = 0;
        if (checks.has(CheckFlag::PARTICLE_COUNT)) {
            this->updateCell(i, colIdx++, fragment.getParticleCnt());
        }

        if (checks.hasAny(CheckFlag::MASS_FRACTION, CheckFlag::DIAMETER)) {
            const Pair<Float> massAndDiameter = getMassAndDiameter(fragment);
            if (checks.has(CheckFlag::MASS_FRACTION)) {
                this->updateCell(i, colIdx++, massAndDiameter[0] / totalMass);
            }
            if (checks.has(CheckFlag::DIAMETER)) {
                this->updateCell(i, colIdx++, massAndDiameter[1] / 1.e3_f);
            }
        }

        if (checks.has(CheckFlag::VELOCITY_DIFFERENCE)) {
            this->updateCell(i, colIdx++, getVelocityDifference(fragment, lr));
        }

        if (checks.has(CheckFlag::PERIOD)) {
            const Float period = getPeriod(fragment);
            this->updateCell(i, colIdx++, period);
        }

        if (checks.hasAny(CheckFlag::RATIO_BA, CheckFlag::RATIO_CB)) {
            const Pair<Float> ratios = getSemiaxisRatios(fragment);
            if (checks.has(CheckFlag::RATIO_CB)) {
                this->updateCell(i, colIdx++, ratios[0]);
            }
            if (checks.has(CheckFlag::RATIO_BA)) {
                this->updateCell(i, colIdx++, ratios[1]);
            }
        }

        if (checks.has(CheckFlag::SPHERICITY)) {
            this->updateCell(i, colIdx++, getSphericity(fragment));
        }

        if (checks.has(CheckFlag::MOONS)) {
            ArrayView<const Float> masses = getter.getMasses();
            ArrayView<const Vector> positions = getter.getPositions();
            ArrayView<const Vector> velocities = getter.getVelocities();
            const Size count =
                getMoons(masses, positions, velocities, i, config.moonLimit, config.radiiLimit);
            this->updateCell(i, colIdx++, count);
        }

        executeOnMainThread([this] { grid->AutoSize(); });
    }

    executeOnMainThread([this] { thread.join(); });
}

template <typename T>
void GridPage::updateCell(const Size rowIdx, const Size colIdx, const T& value) {
    executeOnMainThread([this, rowIdx, colIdx, value] { //
        grid->SetCellValue(rowIdx, colIdx, std::to_string(value));
    });
}

wxCheckBox* GridPage::getCheck(const CheckFlag check) const {
    wxCheckBox* box = dynamic_cast<wxCheckBox*>(this->FindWindow(int(check)));
    ASSERT(box);
    return box;
}

Size GridPage::getCheckedCount() const {
    Size count = 0;
    const StaticArray<CheckFlag, CHECK_COUNT> allIds = {
        CheckFlag::PARTICLE_COUNT,
        CheckFlag::MASS_FRACTION,
        CheckFlag::DIAMETER,
        CheckFlag::VELOCITY_DIFFERENCE,
        CheckFlag::PERIOD,
        CheckFlag::RATIO_CB,
        CheckFlag::RATIO_BA,
        CheckFlag::SPHERICITY,
        CheckFlag::MOONS,
    };
    ASSERT(allIds.size() == CHECK_COUNT);

    for (CheckFlag id : allIds) {
        count += int(this->getCheck(id)->GetValue());
    }
    return count;
}

NAMESPACE_SPH_END
