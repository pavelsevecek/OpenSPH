#include "gui/windows/NodePage.h"
#include "gui/Utils.h"
#include "gui/objects/CameraJobs.h"
#include "gui/objects/DelayedCallback.h"
#include "gui/objects/RenderJobs.h"
#include "gui/windows/BatchDialog.h"
#include "gui/windows/CurveDialog.h"
#include "gui/windows/RunSelectDialog.h"
#include "io/FileSystem.h"
#include "objects/utility/IteratorAdapters.h"
#include "run/Config.h"
#include "run/ScriptNode.h"
#include "run/SpecialEntries.h"
#include "run/jobs/IoJobs.h"
#include "run/jobs/Presets.h"
#include "run/jobs/ScriptJobs.h"
#include "thread/CheckFunction.h"
#include <wx/dcclient.h>
#include <wx/dirdlg.h>
#include <wx/graphics.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/richtooltip.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/treectrl.h>

#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/props.h>
// has to be included last
#include <wx/propgrid/advprops.h>

#include <wx/aui/framemanager.h>

NAMESPACE_SPH_BEGIN

constexpr int FIRST_SLOT_Y = 60;
constexpr int SLOT_DY = 25;
constexpr int SLOT_RADIUS = 6;

/// \todo figure out why this is needed
static AnimationJob animationDummy("dummy");
static PerspectiveCameraJob cameraDummy("dummy");

#ifdef SPH_USE_CHAISCRIPT
static ChaiScriptJob scriptDummy("dummy");
#endif

//-----------------------------------------------------------------------------------------------------------
// NodeManager
//-----------------------------------------------------------------------------------------------------------

NodeManager::NodeManager(NodeEditor* editor, SharedPtr<INodeManagerCallbacks> callbacks)
    : editor(editor)
    , callbacks(callbacks) {
    globals.set(RunSettingsId::SPH_KERNEL, KernelEnum::CUBIC_SPLINE)
        .set(RunSettingsId::GENERATE_UVWS, false)
        .set(RunSettingsId::UVW_MAPPING, UvMapEnum::SPHERICAL)
        .set(RunSettingsId::RUN_RNG, RngEnum::UNIFORM)
        .set(RunSettingsId::RUN_RNG_SEED, 1234)
        .set(RunSettingsId::RUN_THREAD_CNT, 0)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 1000)
        .set(RunSettingsId::FINDER_LEAF_SIZE, 25)
        .set(RunSettingsId::FINDER_MAX_PARALLEL_DEPTH, 50)
        .set(RunSettingsId::RUN_AUTHOR, std::string("Pavel Sevecek"))
        .set(RunSettingsId::RUN_COMMENT, std::string(""))
        .set(RunSettingsId::RUN_EMAIL, std::string("sevecek@sirrah.troja.mff.cuni.cz"));
}

VisNode* NodeManager::addNode(const SharedPtr<JobNode>& node, const Pixel position) {
    const std::string currentName = node->instanceName();
    UniqueNameManager nameMgr = this->makeUniqueNameManager();
    const std::string fixedName = nameMgr.getName(currentName);
    if (fixedName != currentName) {
        VirtualSettings settings = node->getSettings();
        settings.set("name", fixedName);
    }

    VisNode vis(node.get(), position);
    VisNode& stored = nodes.insert(node, vis);
    editor->Refresh();
    return &stored;
}

VisNode* NodeManager::addNode(const SharedPtr<JobNode>& node) {
    const wxSize size = editor->GetSize();
    return this->addNode(node, Pixel(size.x / 2, size.y / 2) - editor->offset());
}

void NodeManager::addNodes(JobNode& node) {
    node.enumerate([this](SharedPtr<JobNode> node, Size UNUSED(depth)) {
        nodes.insert(node, VisNode(node.get(), Pixel(0, 0)));
    });
    this->layoutNodes(node, Pixel(800, 200) - editor->offset());
    callbacks->markUnsaved(true);
}

void NodeManager::cloneHierarchy(JobNode& node) {
    /// \todo deduplicate
    const wxSize size = editor->GetSize();
    const Pixel pivot = Pixel(size.x / 2, size.y / 2) - editor->offset();
    const Pixel offset = pivot - nodes[node.sharedFromThis()].position;

    SharedPtr<JobNode> clonedRoot = Sph::cloneHierarchy(node);
    this->addNodes(*clonedRoot);

    // fix positions
    Array<SharedPtr<JobNode>> origTree, clonedTree;
    node.enumerate([&origTree](SharedPtr<JobNode> node, Size UNUSED(depth)) { //
        origTree.push(node);
    });
    clonedRoot->enumerate([&clonedTree](SharedPtr<JobNode> node, Size UNUSED(depth)) { //
        clonedTree.push(node);
    });
    SPH_ASSERT(origTree.size() == clonedTree.size());

    for (Size i = 0; i < origTree.size(); ++i) {
        nodes[clonedTree[i]].position = nodes[origTree[i]].position + offset;
    }
}

void NodeManager::layoutNodes(JobNode& node, const Pixel position) {
    UnorderedMap<SharedPtr<JobNode>, Size> depthMap;

    node.enumerate([&depthMap](SharedPtr<JobNode> node, Size depth) { //
        depthMap.insert(node, depth);
    });

    // fix depth map so that each provider is at least +1 in depth
    bool depthChanged;
    do {
        depthChanged = false;
        for (auto& element : depthMap) {
            SharedPtr<JobNode> node = element.key;

            // find depths of providers
            for (Size i = 0; i < node->getSlotCnt(); ++i) {
                SlotData data = node->getSlot(i);
                if (data.provider && depthMap[data.provider] <= depthMap[node]) {
                    depthMap[data.provider]++;
                    depthChanged = true;
                }
            }
        }
    } while (depthChanged);


    FlatMap<Size, Array<SharedPtr<JobNode>>> depthMapInv;
    for (auto& element : depthMap) {
        const int depth = element.value;
        if (!depthMapInv.contains(depth)) {
            depthMapInv.insert(depth, {});
        }
        depthMapInv[depth].push(element.key);
    }

    for (auto& element : depthMapInv) {
        const int depth = element.key;
        int index = 0;
        for (auto& node : element.value) {
            VisNode& vis = nodes[node];
            vis.position =
                Pixel(position.x - 200 * depth, position.y + 150 * index - (element.value.size() - 1) * 75);
            ++index;
        }
    }

    editor->Refresh();
    callbacks->markUnsaved(true);
}

void NodeManager::deleteNode(JobNode& node) {
    for (Size i = 0; i < node.getSlotCnt(); ++i) {
        if (SharedPtr<JobNode> provider = node.getSlot(i).provider) {
            provider->disconnect(node.sharedFromThis());
        }
    }
    node.disconnectAll();
    nodes.remove(node.sharedFromThis());
    callbacks->markUnsaved(true);
}

void NodeManager::deleteTree(JobNode& node) {
    Array<SharedPtr<JobNode>> toRemove;
    node.enumerate([&toRemove](SharedPtr<JobNode> node, Size UNUSED(depth)) { toRemove.push(node); });
    for (SharedPtr<JobNode> n : toRemove) {
        nodes.remove(n);
    }
    callbacks->markUnsaved(true);
}

void NodeManager::deleteAll() {
    nodes.clear();
    editor->Refresh();
    callbacks->markUnsaved(true);
}

VisNode* NodeManager::getSelectedNode(const Pixel position) {
    // Nodes are drawn in linear order, meaning nodes in the back will be higher in z-order than nodes in the
    // front. To pick the uppermost one, just iterate in reverse.
    for (auto& element : reverse(nodes)) {
        VisNode& node = element.value;
        wxRect rect(wxPoint(node.position), wxPoint(node.position + node.size()));
        if (rect.Contains(wxPoint(position))) {
            return &node;
        }
    }
    return nullptr;
}

NodeSlot NodeManager::getSlotAtPosition(const Pixel position) {
    for (auto& element : nodes) {
        VisNode& node = element.value;
        const Pixel relative = position - node.position;
        for (Size i = 0; i < node.node->getSlotCnt(); ++i) {
            const float dist = getLength(relative - Pixel(0, FIRST_SLOT_Y + i * SLOT_DY));
            if (dist < SLOT_RADIUS) {
                return { &node, i };
            }
        }

        const float dist = getLength(relative - Pixel(node.size().x, FIRST_SLOT_Y));
        if (dist < SLOT_RADIUS) {
            return { &node, NodeSlot::RESULT_SLOT };
        }
    }
    return { nullptr, 0 };
}

class SaveProc : public VirtualSettings::IEntryProc {
private:
    ConfigNode& out;

public:
    explicit SaveProc(ConfigNode& out)
        : out(out) {}

    virtual void onCategory(const std::string& UNUSED(name)) const override {
        // do nothing
    }

    virtual void onEntry(const std::string& name, IVirtualEntry& entry) const override {
        const IVirtualEntry::Type type = entry.getType();
        switch (type) {
        case IVirtualEntry::Type::BOOL:
            out.set<bool>(name, entry.get());
            break;
        case IVirtualEntry::Type::INT:
            out.set<int>(name, entry.get());
            break;
        case IVirtualEntry::Type::FLOAT:
            out.set<Float>(name, entry.get());
            break;
        case IVirtualEntry::Type::VECTOR:
            out.set<Vector>(name, entry.get());
            break;
        case IVirtualEntry::Type::INTERVAL:
            out.set<Interval>(name, entry.get());
            break;
        case IVirtualEntry::Type::STRING:
            out.set<std::string>(name, entry.get());
            break;
        case IVirtualEntry::Type::PATH:
            out.set<Path>(name, entry.get());
            break;
        case IVirtualEntry::Type::ENUM:
        case IVirtualEntry::Type::FLAGS: {
            EnumWrapper ew = entry.get();
            out.set<int>(name, ew.value);
            break;
        }
        case IVirtualEntry::Type::EXTRA: {
            ExtraEntry extra(entry.get());
            out.set<std::string>(name, extra.toString());
            break;
        }
        default:
            NOT_IMPLEMENTED;
        }
    }
};

void NodeManager::save(Config& config) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);

    try {
        SharedPtr<ConfigNode> outGlobals = config.addNode("globals");
        VirtualSettings globalSettings = getGlobalSettings();
        globalSettings.enumerate(SaveProc(*outGlobals));

        SharedPtr<ConfigNode> outNodes = config.addNode("nodes");
        for (auto& element : nodes) {
            const SharedPtr<JobNode> node = element.key;
            const VisNode vis = element.value;

            SharedPtr<ConfigNode> out = outNodes->addChild(node->instanceName());

            out->set("class_name", node->className());
            out->set("position", vis.position);

            // save connected slots
            for (Size i = 0; i < node->getSlotCnt(); ++i) {
                SlotData slot = node->getSlot(i);
                if (SharedPtr<JobNode> provider = slot.provider) {
                    out->set(slot.name, provider->instanceName());
                }
            }

            VirtualSettings settings = node->getSettings();
            settings.enumerate(SaveProc(*out));
        }

        batch.save(config);

    } catch (const Exception& e) {
        wxMessageBox(std::string("Cannot save file.\n\n") + e.what(), "Error", wxOK);
    }
}

class LoadProc : public VirtualSettings::IEntryProc {
private:
    ConfigNode& input;

public:
    LoadProc(ConfigNode& input)
        : input(input) {}

    virtual void onCategory(const std::string& UNUSED(name)) const override {}

    virtual void onEntry(const std::string& name, IVirtualEntry& entry) const override {
        const IVirtualEntry::Type type = entry.getType();

        try {
            switch (type) {
            case IVirtualEntry::Type::BOOL:
                entry.set(input.get<bool>(name));
                break;
            case IVirtualEntry::Type::INT:
                entry.set(input.get<int>(name));
                break;
            case IVirtualEntry::Type::FLOAT:
                entry.set(input.get<Float>(name));
                break;
            case IVirtualEntry::Type::VECTOR:
                entry.set(input.get<Vector>(name));
                break;
            case IVirtualEntry::Type::INTERVAL:
                entry.set(input.get<Interval>(name));
                break;
            case IVirtualEntry::Type::STRING:
                entry.set(input.get<std::string>(name));
                break;
            case IVirtualEntry::Type::PATH:
                entry.set(input.get<Path>(name));
                break;
            case IVirtualEntry::Type::ENUM:
            case IVirtualEntry::Type::FLAGS: {
                EnumWrapper ew = entry.get();
                ew.value = input.get<int>(name);
                entry.set(ew);
                break;
            }
            case IVirtualEntry::Type::EXTRA: {
                /// \todo currently used only by curves, can be generalized if needed
                ExtraEntry extra(makeAuto<CurveEntry>());
                extra.fromString(input.get<std::string>(name));
                entry.set(extra);
                break;
            }
            default:
                NOT_IMPLEMENTED;
            }
        } catch (const Exception& e) {
            /// \todo better logging
            std::cout << "Failed to load value, keeping the default.\n" << e.what() << std::endl;
        }
    }
};


void NodeManager::load(Config& config) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);

    nodes.clear();

    try {
        SharedPtr<ConfigNode> inGlobals = config.getNode("globals");
        VirtualSettings globalSettings = this->getGlobalSettings();
        globalSettings.enumerate(LoadProc(*inGlobals));

        SharedPtr<ConfigNode> inNodes = config.getNode("nodes");
        Array<Tuple<SharedPtr<JobNode>, std::string, std::string>> allToConnect;
        inNodes->enumerateChildren([this, &allToConnect](std::string name, ConfigNode& input) {
            RawPtr<IJobDesc> desc;
            try {
                const std::string className = input.get<std::string>("class_name");
                desc = getJobDesc(className);
                if (!desc) {
                    throw Exception("cannot find desc for node '" + className + "'");
                }
            } catch (const Exception& e) {
                wxMessageBox(e.what(), "Error", wxOK);
                return;
            }

            SharedPtr<JobNode> node = makeShared<JobNode>(desc->create(name));
            this->addNode(node, input.get<Pixel>("position"));
            VirtualSettings settings = node->getSettings();
            settings.enumerate(LoadProc(input));

            for (Size i = 0; i < node->getSlotCnt(); ++i) {
                const std::string slotName = node->getSlot(i).name;
                Optional<std::string> connectedName = input.tryGet<std::string>(slotName);
                if (connectedName) {
                    allToConnect.push(makeTuple(node, slotName, connectedName.value()));
                }
            }
        });

        for (auto& toConnect : allToConnect) {
            for (auto& element : nodes) {
                if (element.key->instanceName() == toConnect.get<2>()) {
                    element.key->connect(toConnect.get<0>(), toConnect.get<1>());
                }
            }
        }

        Array<SharedPtr<JobNode>> nodeList;
        for (auto& pair : nodes) {
            nodeList.push(pair.key);
        }
        batch.load(config, nodeList);

    } catch (const Exception& e) {
        wxMessageBox(std::string("Cannot load file.\n\n") + e.what(), "Error", wxOK);
    }
}

void NodeManager::startRun(JobNode& node) {
    // clone all nodes to avoid touching the data while the simulation is running
    callbacks->startRun(Sph::cloneHierarchy(node, std::string("")), globals, node.instanceName());
}

/// \todo refactor, derive from INode instead
class BatchJob : public IParticleJob {
private:
    Size runCnt;

public:
    BatchJob(const std::string& name, const Size runCnt)
        : IParticleJob(name)
        , runCnt(runCnt) {}

    virtual std::string className() const override {
        return "batch run";
    }

    virtual UnorderedMap<std::string, ExtJobType> getSlots() const override {
        UnorderedMap<std::string, ExtJobType> map;
        for (Size i = 0; i < runCnt; ++i) {
            map.insert("job " + std::to_string(i), JobType::PARTICLES);
        }
        return map;
    }

    virtual VirtualSettings getSettings() override {
        NOT_IMPLEMENTED;
    }

    virtual void evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) override {
        // only used for run the dependencies, the job itself is empty
    }
};

void NodeManager::startBatch(JobNode& node) {
    RawPtr<IJobDesc> desc = getJobDesc(node.className());
    SPH_ASSERT(desc);

    // validate
    for (Size col = 0; col < batch.getParamCount(); ++col) {
        if (!batch.getParamNode(col)) {
            wxMessageBox(std::string(
                "Incomplete set up of batch run.\nSet up all parameters in Project / Batch Run."));
            return;
        }
    }

    Array<SharedPtr<JobNode>> batchNodes;
    try {
        for (Size runIdx = 0; runIdx < batch.getRunCount(); ++runIdx) {
            SharedPtr<JobNode> runNode = Sph::cloneHierarchy(node, batch.getRunName(runIdx) + " / ");
            batch.modifyHierarchy(runIdx, *runNode);
            batchNodes.push(runNode);
        }
    } catch (const Exception& e) {
        wxMessageBox(std::string("Cannot start batch run.\n\n") + e.what(), "Error", wxOK);
    }

    SharedPtr<JobNode> root = makeNode<BatchJob>("batch", batchNodes.size());
    for (Size i = 0; i < batchNodes.size(); ++i) {
        batchNodes[i]->connect(root, "job " + std::to_string(i));
    }

    callbacks->startRun(root, globals, root->instanceName());
}

void NodeManager::startScript(const Path& file) {
#ifdef SPH_USE_CHAISCRIPT
    Array<SharedPtr<JobNode>> rootNodes = getRootNodes();
    Array<SharedPtr<JobNode>> clonedNodes;
    for (const auto& node : rootNodes) {
        SharedPtr<JobNode> cloned = Sph::cloneHierarchy(*node, std::string());
        cloned->enumerate([&](SharedPtr<JobNode> job, Size UNUSED(depth)) { clonedNodes.push(job); });
    }
    SharedPtr<ScriptNode> node = makeShared<ScriptNode>(file, std::move(clonedNodes));

    callbacks->startRun(node, globals, "Script '" + file.native() + "'");
#else
    throw InvalidSetup("Cannot start script '" + file.native() + "', no ChaiScript support.");
#endif
}

void NodeManager::startAll() {
    Array<SharedPtr<JobNode>> inputs;
    for (auto& element : nodes) {
        SharedPtr<JobNode> node = element.key;
        Optional<ExtJobType> provided = node->provides();
        if ((!provided || provided.value() == JobType::PARTICLES) && node->getDependentCnt() == 0) {
            inputs.push(Sph::cloneHierarchy(*node, std::string("")));
        }
    }
    if (inputs.empty()) {
        wxMessageBox("No simulations to start.");
        return;
    }

    SharedPtr<JobNode> root = makeNode<BatchJob>("batch", inputs.size());
    for (Size i = 0; i < inputs.size(); ++i) {
        inputs[i]->connect(root, "job " + std::to_string(i));
    }

    callbacks->startRun(root, globals, root->instanceName());
}

Array<SharedPtr<JobNode>> NodeManager::getRootNodes() const {
    Array<SharedPtr<JobNode>> inputs;
    for (auto& element : nodes) {
        SharedPtr<JobNode> node = element.key;
        Optional<ExtJobType> provided = node->provides();
        if ((!provided || provided.value() == JobType::PARTICLES) && node->getDependentCnt() == 0) {
            inputs.push(node);
        }
    }
    return inputs;
}

VirtualSettings NodeManager::getGlobalSettings() {
    VirtualSettings settings;

    VirtualSettings::Category& sphCat = settings.addCategory("SPH parameters");
    sphCat.connect<EnumWrapper>("SPH kernel", globals, RunSettingsId::SPH_KERNEL);

    VirtualSettings::Category& parallelCat = settings.addCategory("Parallelization");
    parallelCat.connect<int>("Number of threads", globals, RunSettingsId::RUN_THREAD_CNT);
    parallelCat.connect<int>("Particle granularity", globals, RunSettingsId::RUN_THREAD_GRANULARITY);
    parallelCat.connect<int>("K-d tree leaf size", globals, RunSettingsId::FINDER_LEAF_SIZE);
    parallelCat.connect<int>("Max parallel depth", globals, RunSettingsId::FINDER_MAX_PARALLEL_DEPTH);

    VirtualSettings::Category& flawCat = settings.addCategory("Random numbers");
    flawCat.connect<EnumWrapper>("Random-number generator", globals, RunSettingsId::RUN_RNG);
    flawCat.connect<int>("Random seed", globals, RunSettingsId::RUN_RNG_SEED);

    VirtualSettings::Category& renderCat = settings.addCategory("Rendering");
    renderCat.connect<bool>("Enable textures", globals, RunSettingsId::GENERATE_UVWS);
    renderCat.connect<EnumWrapper>("UV mapping", globals, RunSettingsId::UVW_MAPPING);

    VirtualSettings::Category& authorCat = settings.addCategory("Run metadata");
    authorCat.connect<std::string>("Author name", globals, RunSettingsId::RUN_AUTHOR);
    authorCat.connect<std::string>("Author e-mail", globals, RunSettingsId::RUN_EMAIL);
    authorCat.connect<std::string>("Comment", globals, RunSettingsId::RUN_COMMENT);

    return settings;
}

UniqueNameManager NodeManager::makeUniqueNameManager() const {
    Array<std::string> names;
    for (auto& element : nodes) {
        names.push(element.key->instanceName());
    }

    UniqueNameManager uniqueNames(names);
    return uniqueNames;
}

void NodeManager::showBatchDialog() {
    Array<SharedPtr<JobNode>> nodeList;
    for (auto& pair : nodes) {
        nodeList.push(pair.key);
    }
    BatchDialog* batchDialog = new BatchDialog(editor, batch, std::move(nodeList));
    if (batchDialog->ShowModal() == wxID_OK) {
        batch = batchDialog->getBatch().clone();
        callbacks->markUnsaved(true);
    }
    batchDialog->Destroy();
}

void NodeManager::selectRun() {
    SharedPtr<JobNode> node = activeNode.lock();
    if (node) {
        callbacks->startRun(node, globals, node->instanceName());
        return;
    }

    Array<SharedPtr<JobNode>> nodeList = getRootNodes();
    if (nodeList.empty()) {
        wxMessageBox(std::string("No simulation nodes added. First, create a simulation by double-clicking "
                                 "an item in the node list on the right side."),
            "No runs",
            wxOK);
        return;
    }

    if (nodeList.size() == 1) {
        // only a single node, no need for run select dialog
        SharedPtr<JobNode> node = nodeList.front();
        callbacks->startRun(node, globals, node->instanceName());
        return;
    }

    RunSelectDialog* dialog = new RunSelectDialog(editor, std::move(nodeList));
    if (dialog->ShowModal() == wxID_OK) {
        node = dialog->selectedNode();
        if (dialog->remember()) {
            activeNode = node;
        }
        callbacks->startRun(node, globals, node->instanceName());
    }
    dialog->Destroy();
}

//-----------------------------------------------------------------------------------------------------------
// NodeEditor
//-----------------------------------------------------------------------------------------------------------

NodeEditor::NodeEditor(NodeWindow* parent, SharedPtr<INodeManagerCallbacks> callbacks)
    : wxPanel(parent, wxID_ANY)
    , callbacks(callbacks)
    , nodeWindow(parent) {
    this->SetMinSize(wxSize(1024, 768));

    this->Connect(wxEVT_PAINT, wxPaintEventHandler(NodeEditor::onPaint));
    this->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(NodeEditor::onMouseWheel));
    this->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(NodeEditor::onLeftDown));
    this->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(NodeEditor::onLeftUp));
    this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(NodeEditor::onRightUp));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(NodeEditor::onMouseMotion));
    this->Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(NodeEditor::onDoubleClick));
}

static void drawCenteredText(wxGraphicsContext* gc,
    const std::string& text,
    const Pixel from,
    const Pixel to) {
    wxDouble width, height, descent, externalLeading;
    gc->GetTextExtent(text, &width, &height, &descent, &externalLeading);
    const Pixel pivot = (from + to) / 2 - Pixel(int(width), int(height)) / 2;
    gc->DrawText(text, pivot.x, pivot.y);
}

Pixel NodeSlot::position() const {
    if (index == RESULT_SLOT) {
        return vis->position + Pixel(VisNode::SIZE_X, FIRST_SLOT_Y);
    } else {
        return vis->position + Pixel(0, FIRST_SLOT_Y + SLOT_DY * index);
    }
}


static wxColour getLineColor(const Rgba& background) {
    if (background.intensity() > 0.5f) {
        // light theme
        return wxColour(30, 30, 30);
    } else {
        // dark theme
        return wxColour(230, 230, 230);
    }
}

static FlatMap<ExtJobType, wxPen> NODE_PENS_DARK = [] {
    FlatMap<ExtJobType, wxPen> pens;
    wxPen& storagePen = pens.insert(JobType::PARTICLES, *wxBLACK_PEN);
    storagePen.SetColour(wxColour(230, 230, 230));

    wxPen& materialPen = pens.insert(JobType::MATERIAL, *wxBLACK_PEN);
    materialPen.SetColour(wxColour(255, 120, 60));
    materialPen.SetStyle(wxPENSTYLE_SHORT_DASH);

    wxPen& geometryPen = pens.insert(JobType::GEOMETRY, *wxBLACK_PEN);
    geometryPen.SetColour(wxColour(60, 120, 255));
    geometryPen.SetStyle(wxPENSTYLE_SHORT_DASH);

    wxPen& cameraPen = pens.insert(GuiJobType::CAMERA, *wxBLACK_PEN);
    cameraPen.SetColour(wxColour(150, 225, 100));
    cameraPen.SetStyle(wxPENSTYLE_SHORT_DASH);
    return pens;
}();

static FlatMap<ExtJobType, wxPen> NODE_PENS_LIGHT = [] {
    FlatMap<ExtJobType, wxPen> pens;
    wxPen& storagePen = pens.insert(JobType::PARTICLES, *wxBLACK_PEN);
    storagePen.SetColour(wxColour(30, 30, 30));

    wxPen& materialPen = pens.insert(JobType::MATERIAL, *wxBLACK_PEN);
    materialPen.SetColour(wxColour(150, 40, 10));
    materialPen.SetStyle(wxPENSTYLE_SHORT_DASH);

    wxPen& geometryPen = pens.insert(JobType::GEOMETRY, *wxBLACK_PEN);
    geometryPen.SetColour(wxColour(0, 20, 80));
    geometryPen.SetStyle(wxPENSTYLE_SHORT_DASH);

    wxPen& cameraPen = pens.insert(GuiJobType::CAMERA, *wxBLACK_PEN);
    cameraPen.SetColour(wxColour(80, 10, 10));
    cameraPen.SetStyle(wxPENSTYLE_SHORT_DASH);
    return pens;
}();

static wxPen& getNodePen(const ExtJobType type, const bool isLightTheme) {
    if (isLightTheme) {
        return NODE_PENS_LIGHT[type];
    } else {
        return NODE_PENS_DARK[type];
    }
}

static Rgba decreaseContrast(const Rgba& color, const float amount, const bool darken) {
    if (darken) {
        return color.darken(amount);
    } else {
        return color.brighten(3.f * amount);
    }
}

static void drawCurve(wxGraphicsContext* gc, const Pixel from, const Pixel to) {
    wxGraphicsPath path = gc->CreatePath();
    path.MoveToPoint(from.x, from.y);

    const Pixel dp = to - from;
    path.AddCurveToPoint(from.x + dp.x / 2, from.y, from.x + dp.x / 2, to.y, to.x, to.y);

    gc->StrokePath(path);
}

static bool canConnectSlots(const NodeSlot& from, const NodeSlot& to) {
    if (from.vis == to.vis) {
        // connecting to the same node
        /// \todo generalize, avoid circular dependency
        return false;
    }
    if ((from.index == NodeSlot::RESULT_SLOT) == (to.index == NodeSlot::RESULT_SLOT)) {
        // source to source or input to input
        return false;
    }

    if (to.index == NodeSlot::RESULT_SLOT) {
        SPH_ASSERT(from.index != NodeSlot::RESULT_SLOT);
        const SlotData fromSlot = from.vis->node->getSlot(from.index);
        const Optional<ExtJobType> provided = to.vis->node->provides();
        return fromSlot.used && provided && provided.value() == fromSlot.type;
    } else {
        SPH_ASSERT(to.index != NodeSlot::RESULT_SLOT);
        const SlotData toSlot = to.vis->node->getSlot(to.index);
        const Optional<ExtJobType> provided = from.vis->node->provides();
        return toSlot.used && provided && provided.value() == toSlot.type;
    }
}

wxColour NodeEditor::getSlotColor(const NodeSlot& slot, const Rgba& background) {
    if (!state.mousePosition) {
        // paint event called before any mouse event happened, just return the default
        return wxColour(background);
    }

    const Pixel mousePosition = this->transform(state.mousePosition.value());

    if (state.connectingSlot == slot) {
        // connecting source slot, always valid color
        return wxColour(0, 220, 0);
    } else if (getLength(slot.position() - mousePosition) >= SLOT_RADIUS) {
        // not hovered over, background color
        return wxColour(background);
    } else if (state.connectingSlot && !canConnectSlots(state.connectingSlot.value(), slot)) {
        // fail color
        return wxColour(200, 0, 0);
    } else {
        // can connect or just hovering over, valid color
        return wxColour(0, 220, 0);
    }
}

void NodeEditor::paintNode(wxGraphicsContext* gc, const Rgba& background, const VisNode& vis) {
    const Pixel position = vis.position;
    const Pixel size = vis.size();
    const Optional<ExtJobType> provided = vis.node->provides();
    // setup pen and brush
    const bool isLightTheme = background.intensity() > 0.5f;
    wxPen pen = getNodePen(provided.valueOr(JobType::PARTICLES), isLightTheme);
    wxBrush brush = *wxBLACK_BRUSH;
    Rgba brushColor;
    if (!provided || provided.value() == JobType::PARTICLES) {
        brushColor = decreaseContrast(background, 0.1f, isLightTheme);
    } else {
        brushColor = background.blend(Rgba(pen.GetColour()), 0.2f);
    }
    brush.SetColour(wxColour(brushColor));
    gc->SetBrush(brush);
    gc->SetPen(pen);

    const wxFont font = wxSystemSettings::GetFont(wxSYS_SYSTEM_FONT);
    const Rgba lineColor(getLineColor(background));
    gc->SetFont(font, wxColour(lineColor));

    if (&vis == state.activated) {
        pen.SetWidth(3);
        gc->SetPen(pen);
    }

    // node (rounded) rectangle
    gc->DrawRoundedRectangle(position.x, position.y, size.x, size.y, 10);

    // node instance name
    drawCenteredText(gc, vis.node->instanceName(), position, Pixel(position.x + size.x, position.y + 23));

    // node class name
    wxColour disabledTextColor(decreaseContrast(lineColor, 0.3f, !isLightTheme));
    gc->SetFont(font.Smaller(), disabledTextColor);
    drawCenteredText(gc,
        vis.node->className(),
        Pixel(position.x, position.y + 23),
        Pixel(position.x + size.x, position.y + 40));
    gc->SetFont(font, wxColour(lineColor));

    // separating line for particle nodes
    pen = *wxBLACK_PEN;
    if (!provided || provided.value() == JobType::PARTICLES) {
        pen.SetColour(isLightTheme ? wxColour(160, 160, 160) : wxColour(20, 20, 20));
        gc->SetPen(pen);
        const int lineY = 44;
        const int padding = (&vis == state.activated) ? 2 : 1;
        gc->StrokeLine(
            position.x + padding, position.y + lineY, position.x + size.x - padding, position.y + lineY);
        pen.SetColour(isLightTheme ? wxColour(240, 240, 240) : wxColour(100, 100, 100));
        gc->SetPen(pen);
        gc->StrokeLine(position.x + padding,
            position.y + lineY + 1,
            position.x + size.x - padding,
            position.y + lineY + 1);
    }

    // input slots
    for (Size i = 0; i < vis.node->getSlotCnt(); ++i) {
        SlotData slot = vis.node->getSlot(i);
        const Pixel p1 = NodeSlot{ &vis, i }.position();

        brush.SetColour(this->getSlotColor(NodeSlot{ &vis, i }, background));
        gc->SetBrush(brush);

        pen = getNodePen(slot.type, isLightTheme);
        pen.SetStyle(wxPENSTYLE_SOLID);
        pen.SetWidth(1);
        gc->SetPen(pen);
        gc->DrawEllipse(p1.x - SLOT_RADIUS, p1.y - SLOT_RADIUS, 2 * SLOT_RADIUS, 2 * SLOT_RADIUS);

        if (slot.used) {
            gc->SetFont(font, wxColour(lineColor));
        } else {
            gc->SetFont(font, disabledTextColor);
        }
        gc->DrawText(slot.name, p1.x + 14, p1.y - 10);
    }

    // result slot
    if (provided) {
        const Pixel resultSlot(position.x + size.x, position.y + FIRST_SLOT_Y);
        brush.SetColour(this->getSlotColor(NodeSlot{ &vis, NodeSlot::RESULT_SLOT }, background));
        gc->SetBrush(brush);

        pen = getNodePen(provided.value(), isLightTheme);
        pen.SetStyle(wxPENSTYLE_SOLID);
        pen.SetWidth(1);
        gc->SetPen(pen);
        gc->DrawEllipse(
            resultSlot.x - SLOT_RADIUS, resultSlot.y - SLOT_RADIUS, 2 * SLOT_RADIUS, 2 * SLOT_RADIUS);
    }
}

void NodeEditor::save(Config& config) {
    SharedPtr<ConfigNode> out = config.addNode("editor_state");
    out->set("offset", state.offset);
    out->set("zoom", state.zoom);
}

void NodeEditor::load(Config& config) {
    SharedPtr<ConfigNode> in = config.getNode("editor_state");
    state.offset = in->get<Pixel>("offset");
    state.zoom = in->get<float>("zoom");
}

void NodeEditor::paintCurves(wxGraphicsContext* gc, const Rgba& background, const VisNode& vis) {
    const Pixel size = vis.size();

    wxPen pen = *wxBLACK_PEN;
    pen.SetWidth(2);
    pen.SetColour(getLineColor(background));
    gc->SetPen(pen);

    if (state.mousePosition && state.connectingSlot && state.connectingSlot->vis == addressOf(vis)) {
        const Pixel mousePosition = this->transform(state.mousePosition.value());
        const Pixel sourcePoint = state.connectingSlot->position();
        drawCurve(gc, sourcePoint, mousePosition);
    }

    const NodeMap& nodes = nodeMgr->getNodes();
    for (Size i = 0; i < vis.node->getSlotCnt(); ++i) {
        const Pixel p1 = NodeSlot{ &vis, i }.position();
        const SlotData slot = vis.node->getSlot(i);
        if (slot.provider != nullptr) {
            const Pixel childPoint = nodes[slot.provider].position;
            const Pixel p2(childPoint.x + size.x, childPoint.y + FIRST_SLOT_Y);

            drawCurve(gc, p1, p2);
        }
    }
}

void NodeEditor::onPaint(wxPaintEvent& UNUSED(evt)) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
    wxPaintDC dc(this);

    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    if (!gc) {
        return;
    }

    const wxGraphicsMatrix matrix =
        gc->CreateMatrix(state.zoom, 0.f, 0.f, state.zoom, state.offset.x, state.offset.y);
    gc->SetTransform(matrix);

    const NodeMap& nodes = nodeMgr->getNodes();

    const Rgba background(dc.GetBackground().GetColour());

    // first layer - curves
    for (auto& element : nodes) {
        this->paintCurves(gc, background, element.value);
    }

    // second layer to paint over - nodes
    for (auto& element : nodes) {
        this->paintNode(gc, background, element.value);
    }

    delete gc;
}

void NodeEditor::onMouseMotion(wxMouseEvent& evt) {
    const Pixel mousePosition(evt.GetPosition());
    if (evt.Dragging()) {
        if (!state.mousePosition) {
            // unknown position, cannot compute the offset
            state.mousePosition = mousePosition;
            return;
        }

        if (state.selected) {
            // moving a node
            state.selected->position += (mousePosition - state.mousePosition.value()) / state.zoom;
        } else if (!state.connectingSlot) {
            // just drag the editor
            state.offset += mousePosition - state.mousePosition.value();
        }
        this->Refresh();
        callbacks->markUnsaved(false);
    } else {
        const NodeSlot slot = nodeMgr->getSlotAtPosition(this->transform(mousePosition));
        if (slot != state.lastSlot) {
            state.lastSlot = slot;
            this->Refresh();
        }
    }

    state.mousePosition = mousePosition;
}

void NodeEditor::onMouseWheel(wxMouseEvent& evt) {
    constexpr float MAX_ZOOM_OUT = 0.2f;
    constexpr float MAX_ZOOM_IN = 4.f;

    const Pixel position(evt.GetPosition());
    const float spin = evt.GetWheelRotation();
    const float amount = (spin > 0.f) ? 1.2f : 1.f / 1.2f;
    state.zoom = clamp(state.zoom * amount, MAX_ZOOM_OUT, MAX_ZOOM_IN);
    if (state.zoom != MAX_ZOOM_OUT && state.zoom != MAX_ZOOM_IN) {
        state.offset += (position - state.offset) * (1.f - amount);
    }
    this->Refresh();
    callbacks->markUnsaved(false);
}

void NodeEditor::onLeftDown(wxMouseEvent& evt) {
    const Pixel mousePosition(evt.GetPosition());
    const Pixel position = this->transform(mousePosition);

    const NodeSlot slot = nodeMgr->getSlotAtPosition(position);
    if (slot.vis != nullptr) {
        state.connectingSlot = slot;

        if (slot.index != NodeSlot::RESULT_SLOT) {
            SharedPtr<JobNode> node = slot.vis->node->getSlot(slot.index).provider;
            if (node) {
                node->disconnect(slot.vis->node->sharedFromThis());
            }
        }
    } else {
        state.selected = nodeMgr->getSelectedNode(position);
    }
    state.mousePosition = mousePosition;
}

void NodeEditor::onLeftUp(wxMouseEvent& evt) {
    const Pixel mousePosition(evt.GetPosition());
    state.selected = nullptr;

    if (!state.connectingSlot) {
        return;
    }
    NodeSlot sourceSlot = state.connectingSlot.value();

    const Pixel position = this->transform(mousePosition);
    NodeSlot targetSlot = nodeMgr->getSlotAtPosition(position);

    if (targetSlot.vis != nullptr && canConnectSlots(sourceSlot, targetSlot)) {
        if (targetSlot.index == NodeSlot::RESULT_SLOT) {
            std::swap(sourceSlot, targetSlot);
        }

        RawPtr<JobNode> sourceNode = sourceSlot.vis->node;
        RawPtr<JobNode> targetNode = targetSlot.vis->node;

        // disconnect the previous one
        SlotData slotData = targetNode->getSlot(targetSlot.index);
        if (slotData.provider) {
            slotData.provider->disconnect(targetNode->sharedFromThis());
        }

        // connect to the new slot
        sourceNode->connect(targetNode->sharedFromThis(), slotData.name);

        callbacks->markUnsaved(true);
    }

    state.connectingSlot = NOTHING;
    this->Refresh();
}

void NodeEditor::onRightUp(wxMouseEvent& evt) {
    const Pixel position = (Pixel(evt.GetPosition()) - state.offset) / state.zoom;

    wxMenu menu;
    VisNode* vis = nodeMgr->getSelectedNode(position);
    if (vis != nullptr) {
        const Optional<ExtJobType> provided = vis->node->provides();
        if (!provided || provided.value() == JobType::PARTICLES) {
            menu.Append(0, "Evaluate"); // there is no visible result of other types
            menu.Append(1, "Evaluate batch");
        }
    }

    menu.Append(2, "Evaluate all");

    if (vis != nullptr) {
        menu.Append(3, "Clone");
        menu.Append(4, "Clone tree");
        menu.Append(5, "Layout");
        menu.Append(6, "Delete");
        menu.Append(7, "Delete tree");
    }
    menu.Append(8, "Delete all");

    menu.Bind(wxEVT_COMMAND_MENU_SELECTED, [this, vis](wxCommandEvent& evt) {
        const Size index = evt.GetId();
        switch (index) {
        case 0:
            try {
                nodeMgr->startRun(*vis->node);
            } catch (const Exception& e) {
                wxMessageBox(std::string("Cannot run the node: ") + e.what(), "Error", wxOK);
            }
            break;
        case 1:
            nodeMgr->startBatch(*vis->node);
            break;
        case 2:
            nodeMgr->startAll();
            break;
        case 3:
            nodeMgr->addNode(cloneNode(*vis->node));
            break;
        case 4:
            nodeMgr->cloneHierarchy(*vis->node);
            break;
        case 5:
            nodeMgr->layoutNodes(*vis->node, vis->position);
            break;
        case 6:
            nodeMgr->deleteNode(*vis->node);
            break;
        case 7:
            nodeMgr->deleteTree(*vis->node);
            break;
        case 8:
            nodeMgr->deleteAll();
            break;
        default:
            NOT_IMPLEMENTED;
        }

        this->Refresh();
    });
    this->PopupMenu(&menu);
}

void NodeEditor::onDoubleClick(wxMouseEvent& evt) {
    const Pixel position(evt.GetPosition());
    VisNode* vis = nodeMgr->getSelectedNode((position - state.offset) / state.zoom);
    if (vis) {
        state.activated = vis;
        this->Refresh();
        nodeWindow->selectNode(*vis->node);
    }
}


//-----------------------------------------------------------------------------------------------------------
// NodeWindow
//-----------------------------------------------------------------------------------------------------------

class DirDialogAdapter : public wxPGEditorDialogAdapter {
public:
    virtual bool DoShowDialog(wxPropertyGrid* UNUSED(propGrid), wxPGProperty* property) override {
        wxDirDialog dialog(nullptr, "Choose directory", "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
        if (dialog.ShowModal() == wxID_OK) {
            wxString path = dialog.GetPath();
            property->SetValue(path);
            this->SetValue(path);
            return true;
        } else {
            return false;
        }
    }
};

class SaveFileDialogAdapter : public wxPGEditorDialogAdapter {
private:
    Array<FileFormat> formats;

public:
    explicit SaveFileDialogAdapter(Array<FileFormat>&& formats)
        : formats(std::move(formats)) {}

    virtual bool DoShowDialog(wxPropertyGrid* UNUSED(propGrid), wxPGProperty* property) override {
        Optional<Path> file = doSaveFileDialog("Save file...", formats.clone());
        if (file) {
            property->SetValue(file->native());
            this->SetValue(file->native());
            return true;
        } else {
            return false;
        }
    }
};


class OpenFileDialogAdapter : public wxPGEditorDialogAdapter {
private:
    Array<FileFormat> formats;

public:
    explicit OpenFileDialogAdapter(Array<FileFormat>&& formats)
        : formats(std::move(formats)) {}

    virtual bool DoShowDialog(wxPropertyGrid* UNUSED(propGrid), wxPGProperty* property) override {
        Optional<Path> file = doOpenFileDialog("Save file...", formats.clone());
        if (file) {
            property->SetValue(file->native());
            this->SetValue(file->native());
            return true;
        } else {
            return false;
        }
    }
};

class FileProperty : public wxFileProperty {
    Function<wxPGEditorDialogAdapter*()> makeAdapter;

public:
    using wxFileProperty::wxFileProperty;

    void setFunc(Function<wxPGEditorDialogAdapter*()> func) {
        makeAdapter = func;
    }

    virtual wxPGEditorDialogAdapter* GetEditorDialog() const override {
        return makeAdapter();
    }
};

class VectorProperty : public wxStringProperty {
private:
    class ComponentProperty : public wxFloatProperty {
    private:
        VectorProperty* parent;

    public:
        ComponentProperty(VectorProperty* parent, const wxString& name, const Vector& value, const Size index)
            : wxFloatProperty(name, wxPG_LABEL, value[index])
            , parent(parent) {}

        virtual void OnSetValue() override {
            wxFloatProperty::OnSetValue();
            parent->update();
        }
    };

    StaticArray<ComponentProperty*, 3> components;
    wxWindow* parent;

public:
    VectorProperty(wxWindow* parent, const wxString& name, const Vector& value)
        : wxStringProperty(name, wxPG_LABEL, "")
        , parent(parent) {
        this->SetFlagRecursively(wxPG_PROP_READONLY, true);

        static StaticArray<std::string, 3> labels = { "X", "Y", "Z" };
        for (Size i = 0; i < 3; ++i) {
            components[i] = new ComponentProperty(this, labels[i], value, i);
            this->AppendChild(components[i]);
        }

        this->update(false);
    }

    Vector getVector() const {
        Vector v;
        for (Size i = 0; i < 3; ++i) {
            v[i] = components[i]->GetValue();
        }
        return v;
    }

    void update(const bool notify = true) {
        wxString value;
        for (Size i = 0; i < 3; ++i) {
            value += components[i]->GetValue().GetString() + ", ";
        }
        value.RemoveLast(2);

        this->SetValue(value);

        if (notify) {
            // SetValue does not notify the grid, so we have to do it manually
            wxPropertyGridEvent evt(wxEVT_PG_CHANGED);
            evt.SetProperty(this);
            parent->GetEventHandler()->ProcessEvent(evt);
        }
    }
};

class IntervalProperty : public wxStringProperty {
private:
    class ComponentProperty : public wxFloatProperty {
    private:
        IntervalProperty* parent;

    public:
        ComponentProperty(IntervalProperty* parent, const wxString& name, const Float value)
            : wxFloatProperty(name, wxPG_LABEL, value)
            , parent(parent) {}

        virtual void OnSetValue() override {
            wxFloatProperty::OnSetValue();
            parent->update();
        }
    };

    StaticArray<ComponentProperty*, 2> components;
    wxWindow* parent;

public:
    IntervalProperty(wxWindow* parent, const wxString& name, const Interval& value)
        : wxStringProperty(name, wxPG_LABEL, "")
        , parent(parent) {
        this->SetFlagRecursively(wxPG_PROP_READONLY, true);

        components[0] = new ComponentProperty(this, "from", value.lower());
        components[1] = new ComponentProperty(this, "to", value.upper());
        for (ComponentProperty* comp : components) {
            this->AppendChild(comp);
        }

        this->update(false);
    }

    Interval getInterval() const {
        return Interval(components[0]->GetValue(), components[1]->GetValue());
    }

    void update(const bool notify = true) {
        wxString value = "[ " + components[0]->GetValue().GetString() + ", " +
                         components[1]->GetValue().GetString() + " ]";
        this->SetValue(value);

        if (notify) {
            // SetValue does not notify the grid, so we have to do it manually
            wxPropertyGridEvent evt(wxEVT_PG_CHANGED);
            evt.SetProperty(this);
            parent->GetEventHandler()->ProcessEvent(evt);
        }
    }
};

class PropertyGrid {
private:
    wxPropertyGrid* grid;

public:
    explicit PropertyGrid(wxPropertyGrid* grid)
        : grid(grid) {}

    wxPGProperty* addCategory(const std::string& name) const {
        return grid->Append(new wxPropertyCategory(name));
    }

    wxPGProperty* addBool(const std::string& name, const bool value) const {
        return grid->Append(new wxBoolProperty(name, wxPG_LABEL, value));
    }

    wxPGProperty* addInt(const std::string& name, const int value) const {
        return grid->Append(new wxIntProperty(name, wxPG_LABEL, value));
    }

    wxPGProperty* addFloat(const std::string& name, const Float value) const {
        return grid->Append(new wxFloatProperty(name, wxPG_LABEL, value));
    }

    wxPGProperty* addVector(const std::string& name, const Vector& value) const {
        wxPGProperty* prop = grid->Append(new VectorProperty(grid, name, value));
        grid->Collapse(prop);
        return prop;
    }

    wxPGProperty* addInterval(const std::string& name, const Interval& value) const {
        wxPGProperty* prop = grid->Append(new IntervalProperty(grid, name, value));
        grid->Collapse(prop);
        return prop;
    }

    wxPGProperty* addString(const std::string& name, const std::string& value) const {
        return grid->Append(new wxStringProperty(name, wxPG_LABEL, value));
    }

    wxPGProperty* addPath(const std::string& name,
        const Path& value,
        const IVirtualEntry::PathType type,
        Array<IVirtualEntry::FileFormat>&& formats) const {
        FileProperty* prop = new FileProperty(name, wxPG_LABEL, value.native());
        if (type != IVirtualEntry::PathType::DIRECTORY) {
            prop->setFunc([type, formats = std::move(formats)]() -> wxPGEditorDialogAdapter* {
                if (type == IVirtualEntry::PathType::INPUT_FILE) {
                    return new OpenFileDialogAdapter(formats.clone());
                } else {
                    return new SaveFileDialogAdapter(formats.clone());
                }
            });
        } else {
            prop->setFunc([] { return new DirDialogAdapter(); });
        }
        return grid->Append(prop);
    }

    wxPGProperty* addEnum(const std::string& name, const IVirtualEntry& entry) const {
        return addEnum<wxEnumProperty>(name, entry);
    }

    wxPGProperty* addFlags(const std::string& name, const IVirtualEntry& entry) const {
        return addEnum<wxFlagsProperty>(name, entry);
    }

    wxPGProperty* addExtra(const std::string& name, const ExtraEntry& extra) const {
        RawPtr<CurveEntry> entry = dynamicCast<CurveEntry>(extra.getEntry());
        SPH_ASSERT(entry);
        return grid->Append(new CurveProperty(name, entry->getCurve()));
    }

    void setTooltip(wxPGProperty* prop, const std::string& tooltip) const {
        grid->SetPropertyHelpString(prop, tooltip);
    }

private:
    template <typename TProperty>
    wxPGProperty* addEnum(const std::string& name, const IVirtualEntry& entry) const {
        wxArrayString values;
        wxArrayInt flags;
        EnumWrapper wrapper = entry.get();
        for (int id : EnumMap::getAll(wrapper.index)) {
            EnumWrapper option(id, wrapper.index);
            if (!entry.isValid(option)) {
                continue;
            }
            const std::string rawName = EnumMap::toString(option.value, option.index);
            values.Add(capitalize(replaceAll(rawName, "_", " ")));
            flags.Add(option.value);
        }
        return grid->Append(new TProperty(name, wxPG_LABEL, values, flags, wrapper.value));
    }
};


class AddParamsProc : public VirtualSettings::IEntryProc {
private:
    PropertyGrid wrapper;

    PropertyEntryMap& propertyEntryMap;

public:
    explicit AddParamsProc(wxPropertyGrid* grid, PropertyEntryMap& propertyEntryMapping)
        : wrapper(grid)
        , propertyEntryMap(propertyEntryMapping) {}

    virtual void onCategory(const std::string& name) const override {
        wrapper.addCategory(name);
    }

    virtual void onEntry(const std::string& UNUSED(key), IVirtualEntry& entry) const override {
        wxPGProperty* prop = nullptr;
        const std::string name = entry.getName();
        switch (entry.getType()) {
        case IVirtualEntry::Type::BOOL:
            prop = wrapper.addBool(name, entry.get());
            break;
        case IVirtualEntry::Type::INT:
            prop = wrapper.addInt(name, entry.get());
            break;
        case IVirtualEntry::Type::FLOAT:
            prop = wrapper.addFloat(name, Float(entry.get()));
            break;
        case IVirtualEntry::Type::VECTOR:
            prop = wrapper.addVector(name, entry.get());
            break;
        case IVirtualEntry::Type::INTERVAL:
            prop = wrapper.addInterval(name, entry.get());
            break;
        case IVirtualEntry::Type::STRING:
            prop = wrapper.addString(name, entry.get());
            break;
        case IVirtualEntry::Type::PATH: {
            const Optional<IVirtualEntry::PathType> type = entry.getPathType();
            SPH_ASSERT(type, "No path type set for entry '" + entry.getName() + "'");
            Array<IVirtualEntry::FileFormat> formats = entry.getFileFormats();
            prop = wrapper.addPath(name, entry.get(), type.value(), std::move(formats));
            break;
        }
        case IVirtualEntry::Type::ENUM:
            prop = wrapper.addEnum(name, entry);
            break;
        case IVirtualEntry::Type::FLAGS:
            prop = wrapper.addFlags(name, entry);
            break;
        case IVirtualEntry::Type::EXTRA:
            prop = wrapper.addExtra(name, entry.get());
            break;
        default:
            NOT_IMPLEMENTED;
        }

        const std::string tooltip = entry.getTooltip();
        if (!tooltip.empty()) {
            wrapper.setTooltip(prop, tooltip);
        }

        propertyEntryMap.insert(prop, &entry);

        SPH_ASSERT(propertyEntryMap[prop]->enabled() ||
                   propertyEntryMap[prop]->getType() != IVirtualEntry::Type(20)); // dummy call
    }
};


NodeWindow::NodeWindow(wxWindow* parent, SharedPtr<INodeManagerCallbacks> callbacks)
    : wxPanel(parent, wxID_ANY) {
    aui = makeAuto<wxAuiManager>(this);

    nodeEditor = new NodeEditor(this, callbacks);
    nodeMgr = makeShared<NodeManager>(nodeEditor, callbacks);
    nodeEditor->setNodeMgr(nodeMgr);

    grid = new wxPropertyGrid(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxPG_DEFAULT_STYLE);
    grid->SetExtraStyle(wxPG_EX_HELP_AS_TOOLTIPS);
    grid->SetMinSize(wxSize(300, -1));

    grid->Bind(wxEVT_PG_CHANGED, [this, callbacks](wxPropertyGridEvent& evt) {
        wxPGProperty* prop = evt.GetProperty();
        if (!propertyEntryMap.contains(prop)) {
            // grid being cleared or not listening to this property
            return;
        }

        IVirtualEntry* entry = propertyEntryMap[prop];
        SPH_ASSERT(entry != nullptr);
        wxVariant value = prop->GetValue();

        switch (entry->getType()) {
        case IVirtualEntry::Type::BOOL:
            entry->set(value.GetBool());
            break;
        case IVirtualEntry::Type::INT:
            entry->set(int(value.GetLong()));
            break;
        case IVirtualEntry::Type::FLOAT:
            entry->set(value.GetDouble());
            break;
        case IVirtualEntry::Type::VECTOR: {
            VectorProperty* vector = dynamic_cast<VectorProperty*>(prop);
            SPH_ASSERT(vector);
            entry->set(vector->getVector());
            break;
        }
        case IVirtualEntry::Type::INTERVAL: {
            IntervalProperty* i = dynamic_cast<IntervalProperty*>(prop);
            SPH_ASSERT(i);
            entry->set(i->getInterval());
            break;
        }
        case IVirtualEntry::Type::STRING: {
            std::string stringValue(wxString(value.GetString()));
            /// \todo generalize, using some kind of validator
            if (entry->getName() == "Name") {
                UniqueNameManager nameMgr = nodeMgr->makeUniqueNameManager();
                stringValue = nameMgr.getName(stringValue);
            }
            entry->set(stringValue);
            break;
        }
        case IVirtualEntry::Type::PATH:
            entry->set(Path(std::string(value.GetString())));
            break;
        case IVirtualEntry::Type::ENUM:
        case IVirtualEntry::Type::FLAGS: {
            EnumWrapper ew = entry->get();
            ew.value = value.GetLong();
            entry->set(ew);
            break;
        }
        case IVirtualEntry::Type::EXTRA: {
            CurveProperty* curveProp = dynamic_cast<CurveProperty*>(prop);
            SPH_ASSERT(curveProp != nullptr);
            Curve curve = curveProp->getCurve();
            ExtraEntry extra(makeAuto<CurveEntry>(curve));
            entry->set(extra);
            break;
        }
        default:
            NOT_IMPLEMENTED;
        }

        if (entry->hasSideEffect()) {
            // need to update all properties
            /// \todo alternatively the entry could return the list of properties to update ...
            this->updateProperties();
        } else {
            // only update the enabled/disabled state
            this->updateEnabled(grid);
        }
        nodeEditor->Refresh();
        callbacks->markUnsaved(true);
    });


    wxTreeCtrl* jobView =
        new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT);
    jobView->SetMinSize(wxSize(300, -1));

    wxTreeItemId rootId = jobView->AddRoot("Nodes");

    class JobTreeData : public wxTreeItemData {
        RawPtr<IJobDesc> desc;

    public:
        explicit JobTreeData(RawPtr<IJobDesc> desc)
            : desc(desc) {}

        AutoPtr<IJob> create() const {
            return desc->create(NOTHING);
        }

        std::string tooltip() const {
            return desc->tooltip();
        }
    };

    FlatMap<std::string, wxTreeItemId> categoryItemIdMap;
    for (const AutoPtr<IJobDesc>& desc : enumerateRegisteredJobs()) {
        const std::string& cat = desc->category();
        if (Optional<wxTreeItemId&> id = categoryItemIdMap.tryGet(cat)) {
            jobView->AppendItem(id.value(), desc->className(), -1, -1, new JobTreeData(desc.get()));
        } else {
            wxTreeItemId catId = jobView->AppendItem(rootId, cat);
            jobView->AppendItem(catId, desc->className(), -1, -1, new JobTreeData(desc.get()));
            categoryItemIdMap.insert(cat, catId);
        }
    }

    wxTreeItemId presetsId = jobView->AppendItem(rootId, "presets");
    std::map<wxTreeItemId, Presets::Id> presetsIdMap;
    for (Presets::Id id : EnumMap::getAll<Presets::Id>()) {
        std::string name = replaceAll(EnumMap::toString(id), "_", " ");
        wxTreeItemId itemId = jobView->AppendItem(presetsId, name);
        presetsIdMap[itemId] = id;
    }

    jobView->Bind(wxEVT_MOTION, [this, jobView](wxMouseEvent& evt) {
        wxPoint pos = evt.GetPosition();
        int flags;
        wxTreeItemId id = jobView->HitTest(pos, flags);

        static DelayedCallback callback;
        if (flags & wxTREE_HITTEST_ONITEMLABEL) {
            JobTreeData* data = dynamic_cast<JobTreeData*>(jobView->GetItemData(id));
            if (data) {
                callback.start(600, [this, jobView, id, data, pos] {
                    const wxString name = jobView->GetItemText(id);
                    wxRichToolTip tip(name, setLineBreak(data->tooltip(), 50));
                    const wxRect rect(pos, pos);
                    tip.ShowFor(jobView, &rect);
                    tip.SetTimeout(1e6);

                    nodeEditor->invalidateMousePosition();
                });
            }
        } else {
            callback.stop();
        }
    });

    jobView->Bind(wxEVT_TREE_ITEM_ACTIVATED, [=](wxTreeEvent& evt) {
        wxTreeItemId id = evt.GetItem();
        UniqueNameManager nameMgr = nodeMgr->makeUniqueNameManager();
        if (presetsIdMap.find(id) != presetsIdMap.end()) {
            SharedPtr<JobNode> presetNode = Presets::make(presetsIdMap.at(id), nameMgr);
            nodeMgr->addNodes(*presetNode);
        }

        JobTreeData* data = dynamic_cast<JobTreeData*>(jobView->GetItemData(id));
        if (data) {
            AutoPtr<IJob> job = data->create();
            if (RawPtr<LoadFileJob> loader = dynamicCast<LoadFileJob>(job.get())) {
                Optional<Path> path = doOpenFileDialog("Load file", getInputFormats());
                if (path) {
                    VirtualSettings settings = loader->getSettings();
                    settings.set("file", path.value());
                    // settings.set("name", "Load '" + path->fileName().native() + "'");
                }
            }
            if (RawPtr<SaveFileJob> saver = dynamicCast<SaveFileJob>(job.get())) {
                Optional<Path> path = doSaveFileDialog("Save file", getOutputFormats());
                if (path) {
                    VirtualSettings settings = saver->getSettings();
                    settings.set(RunSettingsId::RUN_OUTPUT_NAME, path.value());
                    const Optional<IoEnum> type = getIoEnum(path->extension().native());
                    if (type) {
                        settings.set(RunSettingsId::RUN_OUTPUT_TYPE, EnumWrapper(type.value()));
                    } else {
                        wxMessageBox(
                            "Unknown file extension '" + path->extension().native() + "'", "Error", wxOK);
                        return;
                    }
                }
            }
            SharedPtr<JobNode> node = makeShared<JobNode>(std::move(job));
            VisNode* vis = nodeMgr->addNode(node);
            nodeEditor->activate(vis);
            this->selectNode(*node);
            callbacks->markUnsaved(true);
        }
    });

    /*wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(grid, 1, wxEXPAND | wxLEFT);
    sizer->Add(mainPanel, 3, wxEXPAND | wxALL);
    sizer->Add(jobView, 1, wxEXPAND | wxRIGHT);
    this->SetSizerAndFit(sizer);*/
    this->SetAutoLayout(true);

    wxAuiPaneInfo info;
    info.Left().MinSize(wxSize(300, -1));
    aui->AddPane(grid, info);
    aui->AddPane(nodeEditor, wxCENTER);

    info.Right();
    aui->AddPane(jobView, info);
    aui->Update();

    panelInfoMap.insert(ID_LIST, &aui->GetPane(jobView));
    panelInfoMap.insert(ID_PROPERTIES, &aui->GetPane(grid));
    /*this->Bind(wxEVT_SIZE, [this](wxSizeEvent& UNUSED(evt)) {
        // this->Fit();
        this->Layout();
    });*/
}

NodeWindow::~NodeWindow() {
    aui->UnInit();
    aui = nullptr;
}

void NodeWindow::showPanel(const PanelId id) {
    panelInfoMap[id]->Show();
    aui->Update();
}

void NodeWindow::selectNode(const JobNode& node) {
    grid->CommitChangesFromEditor();

    settings = node.getSettings();
    this->updateProperties();
}

void NodeWindow::clearGrid() {
    grid->CommitChangesFromEditor();
    grid->Clear();
    propertyEntryMap.clear();
}

void NodeWindow::showGlobals() {
    settings = nodeMgr->getGlobalSettings();
    this->updateProperties();
}

void NodeWindow::showBatchDialog() {
    nodeMgr->showBatchDialog();
}

void NodeWindow::selectRun() {
    nodeMgr->selectRun();
}

void NodeWindow::startScript(const Path& file) {
    nodeMgr->startScript(file);
}

void NodeWindow::reset() {
    nodeMgr->deleteAll();
    this->clearGrid();
}

void NodeWindow::save(Config& config) {
    grid->CommitChangesFromEditor();

    nodeMgr->save(config);
    nodeEditor->save(config);
}

void NodeWindow::load(Config& config) {
    nodeMgr->load(config);
    nodeEditor->load(config);
}

void NodeWindow::addNode(const SharedPtr<JobNode>& node) {
    nodeMgr->addNode(node);
}

void NodeWindow::addNodes(JobNode& node) {
    nodeMgr->addNodes(node);
}


SharedPtr<JobNode> NodeWindow::createNode(AutoPtr<IJob>&& job) {
    SharedPtr<JobNode> node = makeShared<JobNode>(std::move(job));
    nodeMgr->addNode(node);
    return node;
}

void NodeWindow::updateProperties() {
    const wxString states = grid->SaveEditableState(wxPropertyGrid::ScrollPosState);
    grid->Clear();
    propertyEntryMap.clear();

    try {
        AddParamsProc proc(grid, propertyEntryMap);
        settings.enumerate(proc);
    } catch (const Exception& e) {
        SPH_ASSERT(false, e.what());
    }
    this->updateEnabled(grid);

    grid->RestoreEditableState(states, wxPropertyGrid::ScrollPosState);
}

void NodeWindow::updateEnabled(wxPropertyGrid* grid) {
    for (wxPropertyGridIterator iter = grid->GetIterator(); !iter.AtEnd(); iter.Next()) {
        wxPGProperty* prop = iter.GetProperty();
        if (!propertyEntryMap.contains(prop)) {
            continue;
        }

        IVirtualEntry* entry = propertyEntryMap[prop];
        SPH_ASSERT(entry != nullptr);
        const bool enabled = entry->enabled();
        prop->Enable(enabled);
    }
}

UniqueNameManager NodeWindow::makeUniqueNameManager() const {
    return nodeMgr->makeUniqueNameManager();
}

NAMESPACE_SPH_END
