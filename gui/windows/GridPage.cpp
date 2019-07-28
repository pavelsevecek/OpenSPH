#include "gui/windows/GridPage.h"
#include "gui/MainLoop.h"
#include "quantities/Quantity.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"
#include <numeric>
#include <wx/msgdlg.h>

NAMESPACE_SPH_BEGIN

GridPage::GridPage(wxWindow* parent, const wxSize size, const wxSize padding)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size) {

    grid = new wxGrid(this, wxID_ANY, wxDefaultPosition, size);
    grid->CreateGrid(4, 6);
    grid->EnableEditing(false);
    (void)padding;
    this->Layout();
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
        maxIdx = Post::findComponents(storage, 1._f, flags, indices);
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
    grid->SetColLabelValue(0, "Mass fraction");
    grid->SetColLabelValue(1, "Diameter [km]");
    grid->SetColLabelValue(2, "Period [h]");
    grid->SetColLabelValue(3, "Ratio c/b");
    grid->SetColLabelValue(4, "Ratio b/a");
    grid->SetColLabelValue(5, "Sphericity");
    grid->AutoSize();

    thread = std::thread([this, &storage] {
        try {
            this->updateAsync(storage);
        } catch (std::exception& e) {
            executeOnMainThread([message = std::string(e.what())] {
                wxMessageBox("Failed to compute fragment parameters.\n\n" + message, "Fail", wxOK | wxCENTRE);
            });
        }
    });
}

void GridPage::updateAsync(const Storage& storage) {
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    const Float totalMass = std::accumulate(m.begin(), m.end(), 0._f);

    ComponentGetter getter(storage);

    for (Size i = 0; i < 4; ++i) {
        const Storage fragment = getter.getComponent(i);
        if (fragment.getParticleCnt() < 2) {
            break;
        }

        const Pair<Float> massAndDiameter = getMassAndDiameter(fragment);
        this->updateCell(i, 0, massAndDiameter[0] / totalMass);
        this->updateCell(i, 1, massAndDiameter[1] / 1.e3_f);

        const Float period = getPeriod(fragment);
        this->updateCell(i, 2, period);

        const Pair<Float> ratios = getSemiaxisRatios(fragment);
        this->updateCell(i, 3, ratios[0]);
        this->updateCell(i, 4, ratios[1]);

        this->updateCell(i, 5, getSphericity(fragment));

        executeOnMainThread([this] { grid->AutoSize(); });
    }
}

void GridPage::updateCell(const Size rowIdx, const Size colIdx, const Float value) {
    executeOnMainThread([this, rowIdx, colIdx, value] { //
        grid->SetCellValue(rowIdx, colIdx, std::to_string(value));
    });
}

NAMESPACE_SPH_END
