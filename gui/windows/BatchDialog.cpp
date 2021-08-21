#include "gui/windows/BatchDialog.h"
#include "gui/windows/Widgets.h"
#include "run/Config.h"
#include <wx/button.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textdlg.h>

NAMESPACE_SPH_BEGIN

void BatchManager::modifyHierarchy(const Size runIdx, JobNode& node) {
    for (Size colIdx = 0; colIdx < cols.size(); ++colIdx) {
        const String baseName = cols[colIdx].node->instanceName();

        // find the node in the hierarchy
        SharedPtr<JobNode> modifiedNode;
        node.enumerate([&baseName, &modifiedNode](SharedPtr<JobNode> node) {
            const String name = node->instanceName();
            const std::size_t n = name.find(" / ");
            if (n == String::npos) {
                throw InvalidSetup("Invalid name of cloned node: " + name);
            }
            const String clonedName = name.substr(n + 3);
            if (baseName == clonedName) {
                modifiedNode = node;
            }
        });

        if (modifiedNode) {
            this->modifyNode(*modifiedNode, runIdx, colIdx);
        } else {
            throw InvalidSetup("Node '" + baseName + "' not found");
        }
    }
}

/// \todo could be deduplicated (e.g. ConfigValue)
class BatchValueVisitor {
private:
    std::wstringstream ss;

public:
    BatchValueVisitor(const String& newValue)
        : ss(newValue.toUnicode()) {}

    void operator()(Vector& v) {
        ss >> v[X] >> v[Y] >> v[Z];
    }

    void operator()(Interval& i) {
        Float lower, upper;
        ss >> lower >> upper;
        i = Interval(lower, upper);
    }

    void operator()(Path& path) {
        path = Path(String::fromWstring(ss.str()));
    }

    void operator()(EnumWrapper& ew) {
        String str = String::fromWstring(ss.str());
        const Optional<int> value = EnumMap::fromString(str, ew.index);
        if (value) {
            ew.value = value.value();
        } else {
            throw InvalidSetup("Value '{}' is invalid for this parameter. Possible values are:\n{}",
                str,
                EnumMap::getDesc(ew.index));
        }
    }

    void operator()(ExtraEntry& extra) {
        extra.fromString(String::fromWstring(ss.str()));
    }

    /// Default overload
    template <typename T>
    void operator()(T& value) {
        ss >> value;
    }
};

void BatchManager::modifyNode(JobNode& node, const Size runIdx, const Size paramIdx) {
    const String newValue = getCell(paramIdx, runIdx);
    VirtualSettings settings = node.getSettings();
    IVirtualEntry::Value variant = settings.get(cols[paramIdx].key);
    BatchValueVisitor visitor(newValue);
    forValue(variant, visitor);
    settings.set(cols[paramIdx].key, variant);
}

void BatchManager::load(Config& config, ArrayView<const SharedPtr<JobNode>> nodes) {
    SharedPtr<ConfigNode> root = config.getNode("batch");
    const Size rowCnt = root->get<int>("runCount");
    const Size colCnt = root->get<int>("paramCount");
    this->resize(rowCnt, colCnt);

    SharedPtr<ConfigNode> paramNode = root->getChild("params");
    for (Size i = 0; i < colCnt; ++i) {
        const Optional<String> paramDesc = paramNode->tryGet<String>("param-" + toString(i));
        if (!paramDesc) {
            continue;
        }
        const std::size_t sep = paramDesc->find("->");
        if (sep == String::npos) {
            continue;
        }
        cols[i].key = paramDesc->substr(sep + 2);
        const String name = paramDesc->substr(0, sep);
        auto iter = std::find_if(nodes.begin(), nodes.end(), [&name](const SharedPtr<JobNode>& node) {
            return node->instanceName() == name;
        });
        if (iter != nodes.end()) {
            cols[i].node = *iter;
        }
    }

    SharedPtr<ConfigNode> runNode = root->getChild("runs");
    for (Size i = 0; i < rowCnt; ++i) {
        rows[i] = runNode->get<String>("run-" + toString(i));
    }

    SharedPtr<ConfigNode> cellNode = root->getChild("cells");
    for (Size j = 0; j < rowCnt; ++j) {
        for (Size i = 0; i < colCnt; ++i) {
            cells[Pixel(i, j)] = cellNode->get<String>("cell-" + toString(i) + "-" + toString(j));
        }
    }
}

void BatchManager::save(Config& config) {
    SharedPtr<ConfigNode> root = config.addNode("batch");
    root->set("runCount", rows.size());
    root->set("paramCount", cols.size());

    SharedPtr<ConfigNode> paramNode = root->addChild("params");
    for (Size i = 0; i < cols.size(); ++i) {
        if (cols[i].node) {
            paramNode->set("param-" + toString(i), cols[i].node->instanceName() + "->" + cols[i].key);
        }
    }

    SharedPtr<ConfigNode> runNode = root->addChild("runs");
    for (Size i = 0; i < rows.size(); ++i) {
        runNode->set("run-" + toString(i), getRunName(i));
    }

    SharedPtr<ConfigNode> cellNode = root->addChild("cells");
    for (Size j = 0; j < rows.size(); ++j) {
        for (Size i = 0; i < cols.size(); ++i) {
            cellNode->set("cell-" + toString(i) + "-" + toString(j), cells[Pixel(i, j)]);
        }
    }
}

class AddParamProc : public VirtualSettings::IEntryProc {
private:
    wxArrayString& items;
    Array<String>& keys;

public:
    AddParamProc(wxArrayString& items, Array<String>& keys)
        : items(items)
        , keys(keys) {
        keys.clear();
    }

    virtual void onCategory(const String& UNUSED(name)) const override {}

    virtual void onEntry(const String& key, IVirtualEntry& entry) const override {
        keys.push(key);
        items.Add(entry.getName().toUnicode());
    }
};

class ParamSelectDialog : public wxDialog {
private:
    ArrayView<const SharedPtr<JobNode>> nodes;
    ComboBox* nodeBox;
    ComboBox* paramBox;

    struct {
        Array<String> keys;
    } cached;

public:
    ParamSelectDialog(wxWindow* parent, ArrayView<const SharedPtr<JobNode>> nodes)
        : wxDialog(parent, wxID_ANY, "Select parameter")
        , nodes(nodes) {
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

        wxBoxSizer* nodeSizer = new wxBoxSizer(wxHORIZONTAL);
        nodeSizer->Add(new wxStaticText(this, wxID_ANY, "Node:", wxDefaultPosition, wxSize(120, -1)));
        nodeBox = new ComboBox(this, "");
        wxArrayString items;
        for (const SharedPtr<JobNode>& node : nodes) {
            items.Add(node->instanceName().toUnicode());
        }
        nodeBox->Set(items);
        nodeBox->SetSelection(0);
        nodeSizer->Add(nodeBox);
        sizer->Add(nodeSizer);

        wxBoxSizer* paramSizer = new wxBoxSizer(wxHORIZONTAL);
        paramSizer->Add(new wxStaticText(this, wxID_ANY, "Parameter:", wxDefaultPosition, wxSize(120, -1)));
        paramBox = new ComboBox(this, "");
        paramSizer->Add(paramBox);
        sizer->Add(paramSizer);

        nodeBox->Bind(wxEVT_COMBOBOX, [this, nodes](wxCommandEvent& UNUSED(evt)) {
            const int idx = nodeBox->GetSelection();
            SPH_ASSERT(idx < int(nodes.size()));
            VirtualSettings settings = nodes[idx]->getSettings();
            wxArrayString items;
            settings.enumerate(AddParamProc(items, cached.keys));
            paramBox->Set(items);
            paramBox->SetSelection(0);
        });

        wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        wxButton* okButton = new wxButton(this, wxID_ANY, "OK");
        okButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->EndModal(wxID_OK); });
        buttonSizer->Add(okButton);
        wxButton* cancelButton = new wxButton(this, wxID_ANY, "Cancel");
        cancelButton->Bind(
            wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->EndModal(wxID_CANCEL); });
        buttonSizer->Add(cancelButton);
        sizer->Add(buttonSizer);

        this->SetSizer(sizer);
    }

    SharedPtr<JobNode> getNode() const {
        return nodes[nodeBox->GetSelection()];
    }

    String getKey() const {
        return cached.keys[paramBox->GetSelection()];
    }

    wxString getLabel() const {
        return nodeBox->GetValue() + " - " + paramBox->GetValue();
    }
};

BatchDialog::BatchDialog(wxWindow* parent, const BatchManager& mgr, Array<SharedPtr<JobNode>>&& nodes)
    : wxDialog(parent, wxID_ANY, "Batch run", wxDefaultPosition, wxSize(800, 530))
    , manager(mgr.clone())
    , nodes(std::move(nodes)) {

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* controlsSizer = new wxBoxSizer(wxHORIZONTAL);
    controlsSizer->Add(new wxStaticText(this, wxID_ANY, "Run count:"));
    wxSpinCtrl* runSpinner = new wxSpinCtrl(this, wxID_ANY);
    runSpinner->SetValue(manager.getRunCount());
    controlsSizer->Add(runSpinner);
    controlsSizer->Add(new wxStaticText(this, wxID_ANY, "Parameter count:"));
    wxSpinCtrl* paramSpinner = new wxSpinCtrl(this, wxID_ANY);
    paramSpinner->SetValue(manager.getParamCount());
    controlsSizer->Add(paramSpinner);

    sizer->Add(controlsSizer);
    sizer->AddSpacer(10);

    grid = new wxGrid(this, wxID_ANY, wxDefaultPosition, wxSize(800, 450));
    grid->SetDefaultColSize(200);
    grid->SetTabBehaviour(wxGrid::Tab_Wrap);
    grid->CreateGrid(manager.getRunCount(), manager.getParamCount());
    grid->EnableEditing(true);

    sizer->Add(grid);

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* okButton = new wxButton(this, wxID_ANY, "OK");
    okButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->EndModal(wxID_OK); });
    buttonSizer->Add(okButton);
    wxButton* closeButton = new wxButton(this, wxID_ANY, "Cancel");
    closeButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->EndModal(wxID_CANCEL); });
    buttonSizer->Add(closeButton);
    sizer->Add(buttonSizer);

    grid->Bind(wxEVT_GRID_LABEL_LEFT_DCLICK, [this](wxGridEvent& evt) {
        const bool isRow = evt.GetCol() == -1;
        if (isRow) {
            wxTextEntryDialog* dialog = new wxTextEntryDialog(this, "Enter name of the run");
            dialog->SetValue(grid->GetRowLabelValue(evt.GetRow()));
            if (dialog->ShowModal() == wxID_OK) {
                const wxString value = dialog->GetValue();
                grid->SetRowLabelValue(evt.GetRow(), value);
                manager.setRunName(evt.GetRow(), String(value));
            }
        } else {
            ParamSelectDialog* dialog = new ParamSelectDialog(this, this->nodes);
            // dialog->SetValue(grid->GetColLabelValue(evt.GetCol()));
            if (dialog->ShowModal() == wxID_OK) {
                grid->SetColLabelValue(evt.GetCol(), dialog->getLabel());
                manager.setParam(evt.GetCol(), dialog->getNode(), dialog->getKey());
            }
        }
    });
    grid->Bind(wxEVT_GRID_LABEL_RIGHT_CLICK, [this](wxGridEvent& evt) {
        if (evt.GetRow() == -1) {
            return;
        }

        wxMenu menu;
        menu.Append(0, "Duplicate");
        menu.Append(1, "Delete");

        const Size rowIdx = evt.GetRow();
        menu.Bind(wxEVT_COMMAND_MENU_SELECTED, [this, rowIdx](wxCommandEvent& evt) {
            switch (evt.GetId()) {
            case 0:
                manager.duplicateRun(rowIdx);
                grid->InsertRows(rowIdx, 1);
                break;
            case 1:
                manager.deleteRun(rowIdx);
                grid->DeleteRows(rowIdx, 1);
                break;
            default:
                NOT_IMPLEMENTED;
            }
            this->update();
        });

        this->PopupMenu(&menu);
    });

    grid->Bind(wxEVT_GRID_CELL_CHANGED, [this](wxGridEvent& evt) { //
        const String value(grid->GetCellValue(evt.GetRow(), evt.GetCol()));
        manager.setCell(evt.GetCol(), evt.GetRow(), value);
    });

    runSpinner->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent& evt) {
        const int newRunCount = max(evt.GetValue(), 1);
        const int oldRunCount = grid->GetNumberRows();
        if (newRunCount > oldRunCount) {
            grid->InsertRows(oldRunCount, newRunCount - oldRunCount);
        } else {
            grid->DeleteRows(newRunCount, oldRunCount - newRunCount);
        }
        manager.resize(newRunCount, grid->GetNumberCols());
    });
    paramSpinner->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent& evt) {
        const int newParamCount = max(evt.GetValue(), 1);
        const int oldParamCount = grid->GetNumberCols();
        if (newParamCount > oldParamCount) {
            grid->InsertCols(oldParamCount, newParamCount - oldParamCount);
        } else {
            grid->DeleteCols(newParamCount, oldParamCount - newParamCount);
        }
        manager.resize(grid->GetNumberRows(), newParamCount);
    });

    this->update();
    this->SetSizer(sizer);
    this->Layout();
}

BatchDialog::~BatchDialog() {}

void BatchDialog::update() {
    const Size runCnt = manager.getRunCount();
    const Size paramCnt = manager.getParamCount();
    SPH_ASSERT(grid->GetNumberCols() == int(paramCnt));
    SPH_ASSERT(grid->GetNumberRows() == int(runCnt));
    for (Size j = 0; j < runCnt; ++j) {
        grid->SetRowLabelValue(j, manager.getRunName(j).toUnicode());
    }
    for (Size i = 0; i < paramCnt; ++i) {
        SharedPtr<JobNode> node = manager.getParamNode(i);
        if (node) {
            String paramValue = node->instanceName() + " - " + manager.getParamKey(i);
            grid->SetColLabelValue(i, paramValue.toUnicode());
        }
    }
    for (Size j = 0; j < runCnt; ++j) {
        for (Size i = 0; i < paramCnt; ++i) {
            grid->SetCellValue(j, i, manager.getCell(i, j).toUnicode());
        }
    }
}

NAMESPACE_SPH_END
