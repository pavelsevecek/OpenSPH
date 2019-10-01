#include "gui/windows/NodePage.h"
#include "gui/Utils.h"
#include "gui/objects/DelayedCallback.h"
#include "gui/objects/RenderWorkers.h"
#include "gui/windows/CurveDialog.h"
#include "io/FileSystem.h"
#include "objects/utility/IteratorAdapters.h"
#include "run/Config.h"
#include "run/SpecialEntries.h"
#include "run/workers/IoWorkers.h"
#include "run/workers/Presets.h"
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
static AnimationWorker animationDummy("dummy");

//-----------------------------------------------------------------------------------------------------------
// NodeManager
//-----------------------------------------------------------------------------------------------------------

NodeManager::NodeManager(NodeEditor* editor, SharedPtr<INodeManagerCallbacks> callbacks)
    : editor(editor)
    , callbacks(callbacks) {
    globals.set(RunSettingsId::SPH_KERNEL, KernelEnum::CUBIC_SPLINE)
        .set(RunSettingsId::GENERATE_UVWS, false)
        .set(RunSettingsId::RUN_RNG, RngEnum::UNIFORM)
        .set(RunSettingsId::RUN_RNG_SEED, 1234)
        .set(RunSettingsId::RUN_THREAD_CNT, 0)
        .set(RunSettingsId::RUN_THREAD_GRANULARITY, 1000)
        .set(RunSettingsId::RUN_AUTHOR, std::string("Pavel Sevecek"))
        .set(RunSettingsId::RUN_COMMENT, std::string(""))
        .set(RunSettingsId::RUN_EMAIL, std::string("sevecek@sirrah.troja.mff.cuni.cz"));
}

SharedPtr<WorkerNode> NodeManager::addNode(AutoPtr<IWorker>&& worker, const Pixel position) {
    const std::string currentName = worker->instanceName();
    UniqueNameManager nameMgr = this->makeUniqueNameManager();
    const std::string fixedName = nameMgr.getName(currentName);
    if (fixedName != currentName) {
        VirtualSettings settings = worker->getSettings();
        settings.set("name", fixedName);
    }

    SharedPtr<WorkerNode> node = makeShared<WorkerNode>(std::move(worker));

    VisNode vis(node.get(), position);
    nodes.insert(node, vis);
    editor->Refresh();
    return node;
}

SharedPtr<WorkerNode> NodeManager::addNode(AutoPtr<IWorker>&& worker) {
    const wxSize size = editor->GetSize();
    return this->addNode(std::move(worker), Pixel(size.x / 2, size.y / 2) - editor->offset());
}

void NodeManager::addNodes(WorkerNode& node) {
    node.enumerate([this](SharedPtr<WorkerNode> node, Size UNUSED(depth)) {
        nodes.insert(node, VisNode(node.get(), Pixel(0, 0)));
    });
    this->layoutNodes(node, Pixel(800, 200) - editor->offset());
}

SharedPtr<WorkerNode> NodeManager::cloneNode(WorkerNode& node) {
    CHECK_FUNCTION(CheckFunction::NO_THROW);
    RawPtr<IWorkerDesc> desc = getWorkerDesc(node.className());
    ASSERT(desc);

    AutoPtr<IWorker> worker = desc->create("cloned " + node.instanceName());

    class CopyEntriesProc : public VirtualSettings::IEntryProc {
    public:
        VirtualSettings& target;

    public:
        CopyEntriesProc(VirtualSettings& target)
            : target(target) {}

        virtual void onCategory(const std::string& UNUSED(name)) const override {}
        virtual void onEntry(const std::string& name, IVirtualEntry& entry) const override {
            if (name != "name") {
                target.set(name, entry.get());
            }
        }
    };

    VirtualSettings target = worker->getSettings();
    VirtualSettings source = node.getSettings();
    CopyEntriesProc proc(target);
    source.enumerate(proc);

    return this->addNode(std::move(worker));
}

void NodeManager::cloneTree(WorkerNode& node) {
    /// \todo deduplicate
    const wxSize size = editor->GetSize();
    const Pixel pivot = Pixel(size.x / 2, size.y / 2) - editor->offset();
    const Pixel offset = pivot - nodes[node.sharedFromThis()].position;

    // maps original node to cloned nodes
    FlatMap<SharedPtr<WorkerNode>, SharedPtr<WorkerNode>> nodeMap;

    // first, clone all nodes and build up the map
    node.enumerate([this, offset, &nodeMap](SharedPtr<WorkerNode> node, Size UNUSED(depth)) {
        SharedPtr<WorkerNode> clonedNode = this->cloneNode(*node);
        nodes[clonedNode].position = nodes[node].position + offset;

        nodeMap.insert(node, clonedNode);
    });

    // second, connect cloned nodes to get the same hierarchy
    node.enumerate([&nodeMap](SharedPtr<WorkerNode> node, Size UNUSED(depth)) {
        for (Size i = 0; i < node->getSlotCnt(); ++i) {
            SlotData slot = node->getSlot(i);
            if (slot.provider != nullptr) {
                nodeMap[slot.provider]->connect(nodeMap[node], slot.name);
            }
        }
    });
}

void NodeManager::layoutNodes(WorkerNode& node, const Pixel position) {
    UnorderedMap<SharedPtr<WorkerNode>, Size> depthMap;

    node.enumerate([&depthMap](SharedPtr<WorkerNode> node, Size depth) { //
        depthMap.insert(node, depth);
    });

    // fix depth map so that each provider is at least +1 in depth
    bool depthChanged;
    do {
        depthChanged = false;
        for (auto& element : depthMap) {
            SharedPtr<WorkerNode> node = element.key;

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


    FlatMap<Size, Array<SharedPtr<WorkerNode>>> depthMapInv;
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
    callbacks->markUnsaved();
}

void NodeManager::deleteNode(WorkerNode& node) {
    for (Size i = 0; i < node.getSlotCnt(); ++i) {
        if (SharedPtr<WorkerNode> provider = node.getSlot(i).provider) {
            provider->disconnect(node.sharedFromThis());
        }
    }
    node.disconnectAll();
    nodes.remove(node.sharedFromThis());
    callbacks->markUnsaved();
}

void NodeManager::deleteTree(WorkerNode& node) {
    Array<SharedPtr<WorkerNode>> toRemove;
    node.enumerate([&toRemove](SharedPtr<WorkerNode> node, Size UNUSED(depth)) { toRemove.push(node); });
    for (SharedPtr<WorkerNode> n : toRemove) {
        nodes.remove(n);
    }
    callbacks->markUnsaved();
}

void NodeManager::deleteAll() {
    nodes.clear();
    editor->Refresh();
    callbacks->markUnsaved();
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
            const SharedPtr<WorkerNode> node = element.key;
            const VisNode vis = element.value;

            SharedPtr<ConfigNode> out = outNodes->addChild(node->instanceName());

            out->set("class_name", node->className());
            out->set("position", vis.position);

            // save connected slots
            for (Size i = 0; i < node->getSlotCnt(); ++i) {
                SlotData slot = node->getSlot(i);
                if (SharedPtr<WorkerNode> provider = slot.provider) {
                    out->set(slot.name, provider->instanceName());
                }
            }

            VirtualSettings settings = node->getSettings();
            settings.enumerate(SaveProc(*out));
        }
    } catch (Exception& e) {
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
            case IVirtualEntry::Type::STRING:
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
        } catch (Exception& e) {
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
        Array<Tuple<SharedPtr<WorkerNode>, std::string, std::string>> allToConnect;
        inNodes->enumerateChildren([this, &allToConnect](std::string name, ConfigNode& input) {
            RawPtr<IWorkerDesc> desc;
            try {
                const std::string className = input.get<std::string>("class_name");
                desc = getWorkerDesc(className);
                if (!desc) {
                    throw Exception("cannot find desc for node '" + className + "'");
                }
            } catch (Exception& e) {
                wxMessageBox(e.what(), "Error", wxOK);
                return;
            }

            SharedPtr<WorkerNode> node = this->addNode(desc->create(name), input.get<Pixel>("position"));
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
    } catch (Exception& e) {
        wxMessageBox(std::string("Cannot load file.\n\n") + e.what(), "Error", wxOK);
    }
}

void NodeManager::startRun(WorkerNode& node) {
    callbacks->startRun(node, globals);
}

class BatchRunWorker : public IParticleWorker {
private:
    Size runCnt;

public:
    BatchRunWorker(const std::string& name, const Size runCnt)
        : IParticleWorker(name)
        , runCnt(runCnt) {}

    virtual std::string className() const override {
        return "batch run";
    }

    virtual UnorderedMap<std::string, WorkerType> getSlots() const override {
        UnorderedMap<std::string, WorkerType> map;
        for (Size i = 0; i < runCnt; ++i) {
            map.insert("worker " + std::to_string(i), WorkerType::PARTICLES);
        }
        return map;
    }

    virtual VirtualSettings getSettings() override {
        NOT_IMPLEMENTED;
    }

    virtual void evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) override {
        // only used for run the dependencies, the worker itself is empty
    }
};

void NodeManager::startAll() {
    Array<SharedPtr<WorkerNode>> inputs;
    for (auto& element : nodes) {
        SharedPtr<WorkerNode> node = element.key;
        if (node->getDependentCnt() == 0) {
            inputs.push(node);
        }
    }

    SharedPtr<WorkerNode> root = makeNode<BatchRunWorker>("batch", inputs.size());
    for (Size i = 0; i < inputs.size(); ++i) {
        inputs[i]->connect(root, "worker " + std::to_string(i));
    }

    callbacks->startRun(*root, globals);
}

VirtualSettings NodeManager::getGlobalSettings() {
    VirtualSettings settings;

    VirtualSettings::Category& sphCat = settings.addCategory("SPH parameters");
    sphCat.connect<EnumWrapper>("SPH kernel", globals, RunSettingsId::SPH_KERNEL);

    VirtualSettings::Category& parallelCat = settings.addCategory("Parallelization");
    parallelCat.connect<int>("Number of threads", globals, RunSettingsId::RUN_THREAD_CNT);
    parallelCat.connect<int>("Particle granularity", globals, RunSettingsId::RUN_THREAD_GRANULARITY);

    VirtualSettings::Category& flawCat = settings.addCategory("Random numbers");
    flawCat.connect<EnumWrapper>("Random-number generator", globals, RunSettingsId::RUN_RNG);
    flawCat.connect<int>("Random seed", globals, RunSettingsId::RUN_RNG_SEED);

    VirtualSettings::Category& renderCat = settings.addCategory("Rendering");
    renderCat.connect<bool>("Enable textures", globals, RunSettingsId::GENERATE_UVWS);

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


//-----------------------------------------------------------------------------------------------------------
// NodeEditor
//-----------------------------------------------------------------------------------------------------------

NodeEditor::NodeEditor(NodeWindow* parent, SharedPtr<INodeManagerCallbacks> callbacks)
    : wxPanel(parent, wxID_ANY)
    , callbacks(callbacks)
    , nodeWindow(parent) {
    this->SetMinSize(wxSize(1024, 768));

    this->Connect(wxEVT_PAINT, wxPaintEventHandler(NodeEditor::onPaint));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(NodeEditor::onMouseMotion));
    this->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(NodeEditor::onMouseWheel));
    this->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(NodeEditor::onLeftDown));
    this->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(NodeEditor::onLeftUp));
    this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(NodeEditor::onRightUp));
    this->Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(NodeEditor::onDoubleClick));
}

static void drawCenteredText(wxGraphicsContext* gc,
    const std::string& text,
    const Pixel from,
    const Pixel to) {
    wxDouble width, height, descent, externalLeading;
    gc->GetTextExtent(text, &width, &height, &descent, &externalLeading);
    const Pixel pivot = (from + to) / 2 - Pixel(width, height) / 2;
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


static FlatMap<WorkerType, wxPen> NODE_PENS_DARK = [] {
    FlatMap<WorkerType, wxPen> pens;
    wxPen& storagePen = pens.insert(WorkerType::PARTICLES, *wxBLACK_PEN);
    storagePen.SetColour(wxColour(230, 230, 230));

    wxPen& materialPen = pens.insert(WorkerType::MATERIAL, *wxBLACK_PEN);
    materialPen.SetColour(wxColour(255, 120, 60));
    materialPen.SetStyle(wxPENSTYLE_SHORT_DASH);

    wxPen& geometryPen = pens.insert(WorkerType::GEOMETRY, *wxBLACK_PEN);
    geometryPen.SetColour(wxColour(60, 120, 255));
    geometryPen.SetStyle(wxPENSTYLE_SHORT_DASH);
    return pens;
}();

static FlatMap<WorkerType, wxPen> NODE_PENS_LIGHT = [] {
    FlatMap<WorkerType, wxPen> pens;
    wxPen& storagePen = pens.insert(WorkerType::PARTICLES, *wxBLACK_PEN);
    storagePen.SetColour(wxColour(30, 30, 30));

    wxPen& materialPen = pens.insert(WorkerType::MATERIAL, *wxBLACK_PEN);
    materialPen.SetColour(wxColour(150, 40, 10));
    materialPen.SetStyle(wxPENSTYLE_SHORT_DASH);

    wxPen& geometryPen = pens.insert(WorkerType::GEOMETRY, *wxBLACK_PEN);
    geometryPen.SetColour(wxColour(0, 20, 150));
    geometryPen.SetStyle(wxPENSTYLE_SHORT_DASH);
    return pens;
}();

static wxPen& getNodePen(const WorkerType type, const bool isLightTheme) {
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
        ASSERT(from.index != NodeSlot::RESULT_SLOT);
        SlotData fromSlot = from.vis->node->getSlot(from.index);
        return fromSlot.used && to.vis->node->provides() == fromSlot.type;
    } else {
        ASSERT(to.index != NodeSlot::RESULT_SLOT);
        SlotData toSlot = to.vis->node->getSlot(to.index);
        return toSlot.used && from.vis->node->provides() == toSlot.type;
    }
}

wxColour NodeEditor::getSlotColor(const NodeSlot& slot, const Rgba& background) {
    const Pixel mousePosition = this->transform(state.mousePosition);

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

    const bool isLightTheme = background.intensity() > 0.5f;
    wxBrush brush = *wxBLACK_BRUSH;
    Rgba brushColor;
    if (vis.node->provides() == WorkerType::PARTICLES) {
        brushColor = decreaseContrast(background, 0.1f, isLightTheme);
    } else {
        wxPen pen = getNodePen(vis.node->provides(), isLightTheme);
        brushColor = background.blend(Rgba(pen.GetColour()), 0.2f);
    }
    brush.SetColour(wxColour(brushColor));
    gc->SetBrush(brush);
    gc->SetPen(getNodePen(vis.node->provides(), isLightTheme));

    const wxFont font = wxSystemSettings::GetFont(wxSYS_SYSTEM_FONT);
    const Rgba lineColor(getLineColor(background));
    gc->SetFont(font, wxColour(lineColor));

    gc->DrawRoundedRectangle(position.x, position.y, size.x, size.y, 10);
    drawCenteredText(gc, vis.node->instanceName(), position, Pixel(position.x + size.x, position.y + 23));

    wxColour disabledTextColor(decreaseContrast(lineColor, 0.3f, !isLightTheme));
    gc->SetFont(font.Smaller(), disabledTextColor);
    drawCenteredText(gc,
        vis.node->className(),
        Pixel(position.x, position.y + 23),
        Pixel(position.x + size.x, position.y + 40));
    gc->SetFont(font, wxColour(lineColor));

    wxPen pen = *wxBLACK_PEN;
    if (vis.node->provides() == WorkerType::PARTICLES) {
        pen.SetColour(isLightTheme ? wxColour(160, 160, 160) : wxColour(20, 20, 20));
        gc->SetPen(pen);
        const int lineY = 44;
        gc->StrokeLine(position.x + 1, position.y + lineY, position.x + size.x - 1, position.y + lineY);
        pen.SetColour(isLightTheme ? wxColour(240, 240, 240) : wxColour(100, 100, 100));
        gc->SetPen(pen);
        gc->StrokeLine(
            position.x + 1, position.y + lineY + 1, position.x + size.x - 1, position.y + lineY + 1);
    }

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


    const Pixel resultSlot(position.x + size.x, position.y + FIRST_SLOT_Y);

    brush.SetColour(this->getSlotColor(NodeSlot{ &vis, NodeSlot::RESULT_SLOT }, background));
    gc->SetBrush(brush);

    pen = getNodePen(vis.node->provides(), isLightTheme);
    pen.SetStyle(wxPENSTYLE_SOLID);
    pen.SetWidth(1);
    gc->SetPen(pen);
    gc->DrawEllipse(resultSlot.x - SLOT_RADIUS, resultSlot.y - SLOT_RADIUS, 2 * SLOT_RADIUS, 2 * SLOT_RADIUS);
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

    if (state.connectingSlot && state.connectingSlot->vis == addressOf(vis)) {
        const Pixel mousePosition = this->transform(state.mousePosition);
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
        if (state.selected) {
            // moving a node
            state.selected->position += (mousePosition - state.mousePosition) / state.zoom;
        } else if (!state.connectingSlot) {
            // just drag the editor
            state.offset += mousePosition - state.mousePosition;
        }
        this->Refresh();
        callbacks->markUnsaved();
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
    callbacks->markUnsaved();
}

void NodeEditor::onLeftDown(wxMouseEvent& evt) {
    const Pixel mousePosition(evt.GetPosition());
    const Pixel position = this->transform(mousePosition);

    const NodeSlot slot = nodeMgr->getSlotAtPosition(position);
    if (slot.vis != nullptr) {
        state.connectingSlot = slot;

        if (slot.index != NodeSlot::RESULT_SLOT) {
            SharedPtr<WorkerNode> node = slot.vis->node->getSlot(slot.index).provider;
            if (node) {
                node->disconnect(slot.vis->node->sharedFromThis());
            }
        }
    } else {
        state.selected = nodeMgr->getSelectedNode(position);
        state.mouseDownPos = mousePosition;
    }
}

void NodeEditor::onLeftUp(wxMouseEvent& evt) {
    const Pixel mousePosition(evt.GetPosition());
    if (state.selected != nullptr) {
        const Pixel offset = (mousePosition - state.mouseDownPos) / state.zoom;
        nodeMgr->addToUndo([this, node = state.selected, offset] {
            node->position -= offset;
            this->Refresh();
            callbacks->markUnsaved();
        });
    }

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

        RawPtr<WorkerNode> sourceNode = sourceSlot.vis->node;
        RawPtr<WorkerNode> targetNode = targetSlot.vis->node;

        // disconnect the previous one
        SlotData slotData = targetNode->getSlot(targetSlot.index);
        if (slotData.provider) {
            slotData.provider->disconnect(targetNode->sharedFromThis());
        }

        // connect to the new slot
        sourceNode->connect(targetNode->sharedFromThis(), slotData.name);

        callbacks->markUnsaved();
    }

    state.connectingSlot = NOTHING;
    this->Refresh();
}

void NodeEditor::onRightUp(wxMouseEvent& evt) {
    const Pixel position = (Pixel(evt.GetPosition()) - state.offset) / state.zoom;

    wxMenu menu;
    VisNode* vis = nodeMgr->getSelectedNode(position);
    if (vis != nullptr && vis->node->provides() == WorkerType::PARTICLES) {
        menu.Append(0, "Evaluate"); // there is no visible result of other types
    }

    menu.Append(1, "Evaluate all");

    if (vis != nullptr) {
        menu.Append(2, "Clone");
        menu.Append(3, "Clone tree");
        menu.Append(4, "Layout");
        menu.Append(5, "Delete");
        menu.Append(6, "Delete tree");
    }
    menu.Append(7, "Delete all");

    menu.Bind(wxEVT_COMMAND_MENU_SELECTED, [this, vis](wxCommandEvent& evt) {
        const Size index = evt.GetId();
        switch (index) {
        case 0:
            try {
                nodeMgr->startRun(*vis->node);
            } catch (Exception& e) {
                wxMessageBox(std::string("Cannot run the node: ") + e.what(), "Error", wxOK);
            }
            break;
        case 1:
            nodeMgr->startAll();
            break;
        case 2:
            nodeMgr->cloneNode(*vis->node);
            break;
        case 3:
            nodeMgr->cloneTree(*vis->node);
            break;
        case 4:
            nodeMgr->layoutNodes(*vis->node, vis->position);
            break;
        case 5:
            nodeMgr->deleteNode(*vis->node);
            break;
        case 6:
            nodeMgr->deleteTree(*vis->node);
            break;
        case 7:
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
        nodeWindow->selectNode(*vis->node);
    }
}


//-----------------------------------------------------------------------------------------------------------
// NodeWindow
//-----------------------------------------------------------------------------------------------------------

class DialogAdapter : public wxPGEditorDialogAdapter {
public:
    virtual bool DoShowDialog(wxPropertyGrid* UNUSED(propGrid), wxPGProperty* property) override {
        wxDirDialog dialog(nullptr, "Choose directory", "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
        if (dialog.ShowModal() == wxID_OK) {
            wxString path = dialog.GetPath();
            std::cout << property->GetLabel() << "  " << path << std::endl;

            property->SetValue(path);
            this->SetValue(path);
            return true;
        } else {
            return false;
        }
    }
};

class DirProperty : public wxFileProperty {
public:
    using wxFileProperty::wxFileProperty;

    virtual wxPGEditorDialogAdapter* GetEditorDialog() const override {
        return new DialogAdapter();
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

    wxPGProperty* addFloat(const std::string& name, const float value) const {
        return grid->Append(new wxFloatProperty(name, wxPG_LABEL, value));
    }

    wxPGProperty* addVector(const std::string& name, const Vector& value) const {
        wxPGProperty* prop = grid->Append(new VectorProperty(grid, name, value));
        grid->Collapse(prop);
        return prop;
    }

    wxPGProperty* addString(const std::string& name, const std::string& value) const {
        return grid->Append(new wxStringProperty(name, wxPG_LABEL, value));
    }

    wxPGProperty* addPath(const std::string& name, const Path& value) const {
        /// \todo needs better way to determine whether to use save file/load file/directory dialog
        wxPGProperty* prop;
        if (value.extension().empty()) {
            prop = new DirProperty(name, wxPG_LABEL, value.native());
        } else {
            prop = new wxFileProperty(name, wxPG_LABEL, value.native());
        }
        return grid->Append(prop);
    }

    wxPGProperty* addEnum(const std::string& name, const EnumWrapper wrapper) const {
        wxArrayString values;
        for (int id : EnumMap::getAll(wrapper.typeHash)) {
            const std::string rawName = EnumMap::toString(id, wrapper.typeHash);
            values.Add(capitalize(replaceAll(rawName, "_", " ")));
        }
        return grid->Append(new wxEnumProperty(name, wxPG_LABEL, values, wxArrayInt(), wrapper.value));
    }

    wxPGProperty* addFlags(const std::string& name, const EnumWrapper wrapper) const {
        wxArrayString values;
        wxArrayInt flags;
        for (int id : EnumMap::getAll(wrapper.typeHash)) {
            const std::string rawName = EnumMap::toString(id, wrapper.typeHash);
            values.Add(capitalize(replaceAll(rawName, "_", " ")));
            flags.Add(int(id));
        }
        return grid->Append(new wxFlagsProperty(name, wxPG_LABEL, values, flags, wrapper.value));
    }

    wxPGProperty* addExtra(const std::string& name, const ExtraEntry& extra) const {
        RawPtr<CurveEntry> entry = dynamicCast<CurveEntry>(extra.getEntry());
        ASSERT(entry);
        return grid->Append(new CurveProperty(name, entry->getCurve()));
    }

    void setTooltip(wxPGProperty* prop, const std::string& tooltip) const {
        grid->SetPropertyHelpString(prop, tooltip);
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
        case IVirtualEntry::Type::STRING:
            prop = wrapper.addString(name, entry.get());
            break;
        case IVirtualEntry::Type::PATH:
            prop = wrapper.addPath(name, entry.get());
            break;
        case IVirtualEntry::Type::ENUM:
            prop = wrapper.addEnum(name, entry.get());
            break;
        case IVirtualEntry::Type::FLAGS:
            prop = wrapper.addFlags(name, entry.get());
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

        ASSERT(propertyEntryMap[prop]->enabled() ||
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
        ASSERT(entry != nullptr);
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
            ASSERT(vector);
            entry->set(vector->getVector());
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
            ASSERT(curveProp != nullptr);
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
        callbacks->markUnsaved();
    });


    wxTreeCtrl* workerView =
        new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT);
    workerView->SetMinSize(wxSize(300, -1));

    wxTreeItemId rootId = workerView->AddRoot("Nodes");

    class WorkerTreeData : public wxTreeItemData {
        RawPtr<IWorkerDesc> desc;

    public:
        WorkerTreeData(RawPtr<IWorkerDesc> desc)
            : desc(desc) {}

        AutoPtr<IWorker> create() const {
            return desc->create(NOTHING);
        }

        std::string tooltip() const {
            return desc->tooltip();
        }
    };

    FlatMap<std::string, wxTreeItemId> categoryItemIdMap;
    for (const AutoPtr<IWorkerDesc>& desc : enumerateRegisteredWorkers()) {
        const std::string& cat = desc->category();
        if (Optional<wxTreeItemId&> id = categoryItemIdMap.tryGet(cat)) {
            workerView->AppendItem(id.value(), desc->className(), -1, -1, new WorkerTreeData(desc.get()));
        } else {
            wxTreeItemId catId = workerView->AppendItem(rootId, cat);
            workerView->AppendItem(catId, desc->className(), -1, -1, new WorkerTreeData(desc.get()));
            categoryItemIdMap.insert(cat, catId);
        }
    }
    wxTreeItemId presetsId = workerView->AppendItem(rootId, "presets");
    wxTreeItemId collisionsId = workerView->AppendItem(presetsId, "asteroid collision");
    wxTreeItemId fragAndReaccId = workerView->AppendItem(presetsId, "fragmentation & reaccumulation");
    wxTreeItemId crateringId = workerView->AppendItem(presetsId, "cratering");
    wxTreeItemId galaxyId = workerView->AppendItem(presetsId, "galaxy collision");

    workerView->Bind(wxEVT_MOTION, [workerView](wxMouseEvent& evt) {
        wxPoint pos = evt.GetPosition();
        int flags;
        wxTreeItemId id = workerView->HitTest(pos, flags);

        static DelayedCallback callback;
        if (flags & wxTREE_HITTEST_ONITEMLABEL) {
            WorkerTreeData* data = dynamic_cast<WorkerTreeData*>(workerView->GetItemData(id));
            if (data) {
                callback.start(600, [workerView, id, data, pos] {
                    const wxString name = workerView->GetItemText(id);
                    wxRichToolTip tip(name, setLineBreak(data->tooltip(), 50));
                    const wxRect rect(pos, pos);
                    tip.ShowFor(workerView, &rect);
                    tip.SetTimeout(1e6);
                });
            }
        } else {
            callback.stop();
        }
    });

    workerView->Bind(wxEVT_TREE_ITEM_ACTIVATED, [=](wxTreeEvent& evt) {
        wxTreeItemId id = evt.GetItem();
        UniqueNameManager nameMgr = nodeMgr->makeUniqueNameManager();
        if (id == fragAndReaccId) {
            auto node = Presets::makeFragmentationAndReaccumulation(nameMgr);
            nodeMgr->addNodes(*node);
        } else if (id == collisionsId) {
            auto node = Presets::makeAsteroidCollision(nameMgr);
            nodeMgr->addNodes(*node);
        } else if (id == crateringId) {
            auto node = Presets::makeCratering(nameMgr);
            nodeMgr->addNodes(*node);
        } else if (id == galaxyId) {
            auto node = Presets::makeGalaxyCollision(nameMgr);
            nodeMgr->addNodes(*node);
        }
        WorkerTreeData* data = dynamic_cast<WorkerTreeData*>(workerView->GetItemData(id));
        if (data) {
            AutoPtr<IWorker> worker = data->create();
            if (RawPtr<LoadFileWorker> loader = dynamicCast<LoadFileWorker>(worker.get())) {
                Optional<Path> path = doOpenFileDialog("Load file",
                    {
                        { "SPH state files", "ssf" },
                        { "SPH compressed files", "scf" },
                        { "Pkdgrav output files", "bt" },
                        { "Text .tab files", "tab" },
                    });
                if (path) {
                    VirtualSettings settings = loader->getSettings();
                    settings.set("file", path.value());
                    // settings.set("name", "Load '" + path->fileName().native() + "'");
                }
            }
            if (RawPtr<SaveFileWorker> saver = dynamicCast<SaveFileWorker>(worker.get())) {
                Optional<Path> path = doSaveFileDialog("Save file",
                    {
                        { "SPH state file", "ssf" },
                        { "SPH compressed file", "scf" },
                        { "VTK grid file", "vtu" },
                    });
                if (path) {
                    VirtualSettings settings = saver->getSettings();
                    settings.set("run.output.name", path.value());
                    // settings.set("name", "Save to '" + path->fileName().native() + "'");
                }
            }
            SharedPtr<WorkerNode> node = nodeMgr->addNode(std::move(worker));
            this->selectNode(*node);
            callbacks->markUnsaved();
        }
    });

    /*wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(grid, 1, wxEXPAND | wxLEFT);
    sizer->Add(mainPanel, 3, wxEXPAND | wxALL);
    sizer->Add(workerView, 1, wxEXPAND | wxRIGHT);
    this->SetSizerAndFit(sizer);*/
    this->SetAutoLayout(true);

    wxAuiPaneInfo info;
    info.Left().MinSize(wxSize(300, -1));
    aui->AddPane(grid, info);
    aui->AddPane(nodeEditor, wxCENTER);

    info.Right();
    aui->AddPane(workerView, info);
    aui->Update();

    panelInfoMap.insert(ID_LIST, &aui->GetPane(workerView));
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

void NodeWindow::selectNode(const WorkerNode& node) {
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

SharedPtr<WorkerNode> NodeWindow::addNode(AutoPtr<IWorker>&& worker) {
    return nodeMgr->addNode(std::move(worker));
}

void NodeWindow::updateProperties() {
    const wxString states = grid->SaveEditableState(wxPropertyGrid::ScrollPosState);
    grid->Clear();
    propertyEntryMap.clear();

    try {
        AddParamsProc proc(grid, propertyEntryMap);
        settings.enumerate(proc);
    } catch (Exception& e) {
        ASSERT(false, e.what());
    }
    this->updateEnabled(grid);

    grid->SetSplitterPosition(int(0.55 * grid->GetSize().x));
    grid->RestoreEditableState(states, wxPropertyGrid::ScrollPosState);
}

void NodeWindow::updateEnabled(wxPropertyGrid* grid) {
    for (wxPropertyGridIterator iter = grid->GetIterator(); !iter.AtEnd(); iter.Next()) {
        wxPGProperty* prop = iter.GetProperty();
        if (!propertyEntryMap.contains(prop)) {
            continue;
        }

        IVirtualEntry* entry = propertyEntryMap[prop];
        ASSERT(entry != nullptr);
        const bool enabled = entry->enabled();
        prop->Enable(enabled);
    }
}

NAMESPACE_SPH_END
