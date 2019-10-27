#include "gui/windows/GridPage.h"
#include "gui/MainLoop.h"
#include "gui/Utils.h"
#include "io/Path.h"
#include "quantities/Quantity.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"
#include <fstream>
#include <numeric>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

NAMESPACE_SPH_BEGIN

/// Flags used as window ID and as parameters to compute
enum class CheckFlag {
    PARTICLE_COUNT = 1 << 0,
    MASS_FRACTION = 1 << 1,
    DIAMETER = 1 << 2,
    VELOCITY_DIFFERENCE = 1 << 3,
    PERIOD = 1 << 4,
    RATIO_CB = 1 << 5,
    RATIO_BA = 1 << 6,
    SPHERICITY = 1 << 7,
};
const Size CHECK_COUNT = 8;

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

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* computeButton = new wxButton(this, wxID_ANY, "Compute");
    buttonSizer->Add(computeButton);

    wxButton* saveButton = new wxButton(this, wxID_ANY, "Save to file");
    saveButton->Enable(false);
    buttonSizer->Add(saveButton);
    sizer->Add(buttonSizer);

    this->SetSizer(sizer);
    this->Layout();

    computeButton->Bind(wxEVT_BUTTON, [this, size, sizer, saveButton](wxCommandEvent& UNUSED(evt)) {
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

        this->update(this->storage);
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

void GridPage::update(const Storage& storage) {
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

    thread = std::thread([this, &storage, fragmentCnt, checks] {
        try {
            this->updateAsync(storage, fragmentCnt, checks);
        } catch (std::exception& e) {
            executeOnMainThread([message = std::string(e.what())] {
                wxMessageBox("Failed to compute fragment parameters.\n\n" + message, "Fail", wxOK | wxCENTRE);
            });
        }
    });
}

void GridPage::updateAsync(const Storage& storage, const Size fragmentCnt, const Flags<CheckFlag> checks) {
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
    };
    ASSERT(allIds.size() == CHECK_COUNT);

    for (CheckFlag id : allIds) {
        count += int(this->getCheck(id)->GetValue());
    }
    return count;
}

NAMESPACE_SPH_END
