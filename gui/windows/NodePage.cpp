#include "gui/windows/NodePage.h"
#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/Project.h"
#include "gui/Utils.h"
#include "gui/objects/CameraJobs.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/DelayedCallback.h"
#include "gui/objects/PaletteEntry.h"
#include "gui/objects/RenderJobs.h"
#include "gui/windows/BatchDialog.h"
#include "gui/windows/CurveDialog.h"
#include "gui/windows/PaletteProperty.h"
#include "gui/windows/PreviewPane.h"
#include "gui/windows/RenderSetup.h"
#include "gui/windows/RunSelectDialog.h"
#include "gui/windows/Tooltip.h"
#include "gui/windows/Widgets.h"
#include "io/FileSystem.h"
#include "objects/utility/IteratorAdapters.h"
#include "run/Config.h"
#include "run/ScriptNode.h"
#include "run/SpecialEntries.h"
#include "run/jobs/IoJobs.h"
#include "run/jobs/Presets.h"
#include "run/jobs/ScriptJobs.h"
#include "thread/CheckFunction.h"
#include <wx/dcbuffer.h>
#include <wx/dirdlg.h>
#include <wx/graphics.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
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
        .set(RunSettingsId::RUN_AUTHOR, L"Pavel \u0160eve\u010Dek"_s)
        .set(RunSettingsId::RUN_COMMENT, ""_s)
        .set(RunSettingsId::RUN_EMAIL, "sevecek@sirrah.troja.mff.cuni.cz"_s);
}

VisNode* NodeManager::addNode(const SharedPtr<JobNode>& node, const Pixel position) {
    const String currentName = node->instanceName();
    UniqueNameManager nameMgr = this->makeUniqueNameManager();
    const String fixedName = nameMgr.getName(currentName);
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
    node.enumerate([&origTree](SharedPtr<JobNode> node) { //
        origTree.push(node);
    });
    clonedRoot->enumerate([&clonedTree](SharedPtr<JobNode> node) { //
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
            SharedPtr<JobNode> node = element.key();

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
        const int depth = element.value();
        if (!depthMapInv.contains(depth)) {
            depthMapInv.insert(depth, {});
        }
        depthMapInv[depth].push(element.key());
    }

    for (auto& element : depthMapInv) {
        const int depth = element.key();
        int index = 0;
        for (auto& node : element.value()) {
            VisNode& vis = nodes[node];
            vis.position =
                Pixel(position.x - 200 * depth, position.y + 150 * index - (element.value().size() - 1) * 75);
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
    node.enumerate([&toRemove](SharedPtr<JobNode> node) { toRemove.push(node); });
    for (SharedPtr<JobNode> n : toRemove) {
        this->deleteNode(*n);
    }
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
        VisNode& node = element.value();
        wxRect rect(wxPoint(node.position), wxPoint(node.position + node.size()));
        if (rect.Contains(wxPoint(position))) {
            return &node;
        }
    }
    return nullptr;
}

NodeSlot NodeManager::getSlotAtPosition(const Pixel position) {
    for (auto& element : nodes) {
        VisNode& node = element.value();
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

    virtual void onCategory(const String& UNUSED(name)) const override {
        // do nothing
    }

    virtual void onEntry(const String& name, IVirtualEntry& entry) const override {
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
            out.set<String>(name, entry.get());
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
            out.set<String>(name, extra.toString());
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
            const SharedPtr<JobNode> node = element.key();
            const VisNode vis = element.value();

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
        messageBox("Cannot save file.\n\n" + exceptionMessage(e), "Error", wxOK);
    }
}

class LoadProc : public VirtualSettings::IEntryProc {
private:
    const ConfigNode& input;
    Array<RawPtr<IVirtualEntry>>& missingEntries;

public:
    LoadProc(const ConfigNode& input, Array<RawPtr<IVirtualEntry>>& missingEntries)
        : input(input)
        , missingEntries(missingEntries) {}

    virtual void onCategory(const String& UNUSED(name)) const override {}

    virtual void onEntry(const String& name, IVirtualEntry& entry) const override {
        CHECK_FUNCTION(CheckFunction::NO_THROW);
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
                entry.set(input.get<String>(name));
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
                ExtraEntry extra = entry.get();
                extra.fromString(input.get<String>(name));
                entry.set(extra);
                break;
            }
            default:
                NOT_IMPLEMENTED;
            }
        } catch (const Exception& e) {
            /// \todo better logging
            std::wcout << L"Failed to load value, deferring.\n" << exceptionMessage(e) << std::endl;
            // process missing entries after all the other entries have been loaded.
            missingEntries.push(addressOf(entry));
        }
    }
};


void NodeManager::load(Config& config) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);

    nodes.clear();

    try {
        SharedPtr<ConfigNode> inGlobals = config.getNode("globals");
        VirtualSettings globalSettings = this->getGlobalSettings();
        Array<RawPtr<IVirtualEntry>> missingEntries;
        globalSettings.enumerate(LoadProc(*inGlobals, missingEntries));

        SharedPtr<ConfigNode> inNodes = config.getNode("nodes");
        Array<Tuple<SharedPtr<JobNode>, String, String>> allToConnect;
        inNodes->enumerateChildren([this, &allToConnect, &missingEntries](String name, ConfigNode& input) {
            RawPtr<IJobDesc> desc;
            try {
                const String className = input.get<String>("class_name");
                desc = getJobDesc(className);
                if (!desc) {
                    throw Exception("Cannot find desc for node '" + className + "'");
                }
            } catch (const Exception& e) {
                messageBox(exceptionMessage(e), "Error", wxOK);
                return;
            }

            SharedPtr<JobNode> node = makeShared<JobNode>(desc->create(name));
            this->addNode(node, input.get<Pixel>("position"));
            VirtualSettings settings = node->getSettings();
            missingEntries.clear();
            settings.enumerate(LoadProc(input, missingEntries));
            for (RawPtr<IVirtualEntry> entry : missingEntries) {
                entry->setFromFallback();
            }

            for (Size i = 0; i < node->getSlotCnt(); ++i) {
                const String slotName = node->getSlot(i).name;
                Optional<String> connectedName = input.tryGet<String>(slotName);
                if (connectedName) {
                    allToConnect.push(makeTuple(node, slotName, connectedName.value()));
                }
            }
        });

        for (auto& toConnect : allToConnect) {
            for (auto& element : nodes) {
                if (element.key()->instanceName() == toConnect.get<2>()) {
                    element.key()->connect(toConnect.get<0>(), toConnect.get<1>());
                }
            }
        }

        Array<SharedPtr<JobNode>> nodeList;
        for (auto& pair : nodes) {
            nodeList.push(pair.key());
        }
        batch.load(config, nodeList);

    } catch (const Exception& e) {
        messageBox("Cannot load file.\n\n" + exceptionMessage(e), "Error", wxOK);
    }
}

void NodeManager::startRun(JobNode& node) {
    // clone all nodes to avoid touching the data while the simulation is running
    callbacks->startRun(Sph::cloneHierarchy(node, String("")), globals, node.instanceName());
}

void NodeManager::startRender(JobNode& node) {
    callbacks->startRender(Sph::cloneHierarchy(node, String("")), globals, node.instanceName());
}

class BatchNode : public INode {
private:
    Array<SharedPtr<JobNode>> nodes;

public:
    explicit BatchNode(Array<SharedPtr<JobNode>>&& nodes)
        : nodes(std::move(nodes)) {}

    virtual void run(const RunSettings& global, IJobCallbacks& callbacks) override {
        for (const SharedPtr<JobNode>& node : nodes) {
            node->run(global, callbacks);
        }
    }
};

void NodeManager::startBatch(JobNode& node) {
    RawPtr<IJobDesc> desc = getJobDesc(node.className());
    SPH_ASSERT(desc);

    // validate
    for (Size col = 0; col < batch.getParamCount(); ++col) {
        if (!batch.getParamNode(col)) {
            messageBox("Incomplete set up of batch run.\nSet up all parameters in Project / Batch Run.",
                "Error",
                wxOK);
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
        messageBox("Cannot start batch run.\n\n" + exceptionMessage(e), "Error", wxOK);
    }

    SharedPtr<BatchNode> root = makeShared<BatchNode>(std::move(batchNodes));
    callbacks->startRun(root, globals, "Batch");
}

void NodeManager::startScript(const Path& file) {
#ifdef SPH_USE_CHAISCRIPT
    Array<SharedPtr<JobNode>> rootNodes = getRootNodes();
    Array<SharedPtr<JobNode>> clonedNodes;
    for (const auto& node : rootNodes) {
        SharedPtr<JobNode> cloned = Sph::cloneHierarchy(*node, String());
        cloned->enumerate([&](SharedPtr<JobNode> job) { clonedNodes.push(job); });
    }
    SharedPtr<ScriptNode> node = makeShared<ScriptNode>(file, std::move(clonedNodes));

    callbacks->startRun(node, globals, "Script '" + file.string() + "'");
#else
    throw InvalidSetup("Cannot start script '" + file.string() + "', no ChaiScript support.");
#endif
}

Array<SharedPtr<JobNode>> NodeManager::getRootNodes() const {
    Array<SharedPtr<JobNode>> inputs;
    for (auto& element : nodes) {
        SharedPtr<JobNode> node = element.key();
        const ExtJobType provided = node->provides();
        if (provided == JobType::PARTICLES && node->getDependentCnt() == 0) {
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
    authorCat.connect<String>("Author name", globals, RunSettingsId::RUN_AUTHOR);
    authorCat.connect<String>("Author e-mail", globals, RunSettingsId::RUN_EMAIL);
    authorCat.connect<String>("Comment", globals, RunSettingsId::RUN_COMMENT);

    return settings;
}

UniqueNameManager NodeManager::makeUniqueNameManager() const {
    Array<String> names;
    for (auto& element : nodes) {
        names.push(element.key()->instanceName());
    }

    UniqueNameManager uniqueNames(names);
    return uniqueNames;
}

void NodeManager::showBatchDialog() {
    Array<SharedPtr<JobNode>> nodeList;
    for (auto& pair : nodes) {
        nodeList.push(pair.key());
    }
    BatchDialog* batchDialog = new BatchDialog(editor, batch, std::move(nodeList));
    if (batchDialog->ShowModal() == wxID_OK) {
        batch = batchDialog->getBatch().clone();
        callbacks->markUnsaved(true);
    }
    batchDialog->Destroy();
}

PreviewPane* NodeManager::createRenderPreview(wxWindow* parent, JobNode& node) {
    return new PreviewPane(parent, wxDefaultSize, node.sharedFromThis(), globals);
}

void NodeManager::selectRender() {
    SharedPtr<JobNode> node = activeRender.lock();
    if (node) {
        callbacks->startRender(node, globals, node->instanceName());
        return;
    }

    Array<SharedPtr<JobNode>> nodeList;
    for (auto& element : nodes) {
        SharedPtr<JobNode> node = element.key();
        const ExtJobType provided = node->provides();
        if (provided == GuiJobType::IMAGE) {
            nodeList.push(node);
        }
    }
    if (nodeList.empty()) {
        messageBox(
            "No render nodes added. Use 'Setup render' or create a 'Render animation' node manually from the "
            "'Rendering' category.",
            "No renders",
            wxOK);
        return;
    }

    if (nodeList.size() == 1) {
        // only a single node, no need for render select dialog
        SharedPtr<JobNode> node = nodeList.front();
        callbacks->startRender(node, globals, node->instanceName());
        return;
    }

    RunSelectDialog* dialog = new RunSelectDialog(editor, std::move(nodeList), "render");
    if (dialog->ShowModal() == wxID_OK) {
        node = dialog->selectedNode();
        if (dialog->remember()) {
            activeRender = node;
        }
        callbacks->startRender(node, globals, node->instanceName());
    }
    dialog->Destroy();
}

void NodeManager::renderSetup() {
    RenderSetup* dialog = new RenderSetup(editor);
    if (dialog->ShowModal() == wxID_OK) {
        Float scale;
        try {
            AutoPtr<IInput> input = Factory::getInput(dialog->firstFilePath);
            Storage storage;
            Statistics stats;
            Outcome result = input->load(dialog->firstFilePath, storage, stats);
            if (!result) {
                throw InvalidSetup(result.error());
            }
            ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
            Box box;
            for (Size i = 0; i < r.size(); ++i) {
                box.extend(r[i]);
            }
            scale = maxElement(box.size()) * 1.e-3_f; // to km
        } catch (const InvalidSetup& e) {
            messageBox("Cannot setup renderer: " + exceptionMessage(e), "Error", wxOK | wxCENTRE);
            return;
        }

        UniqueNameManager nameMgr = this->makeUniqueNameManager();
        SharedPtr<JobNode> renderNode =
            addNode(makeNode<AnimationJob>(nameMgr.getName("Render")))->node->sharedFromThis();
        VirtualSettings renderSettings = renderNode->getSettings();
        renderSettings.set("directory", dialog->outputDir);
        renderSettings.set(GuiSettingsId::RENDERER, EnumWrapper(dialog->selectedRenderer));
        renderSettings.set("quantity", EnumWrapper(RenderColorizerId::BEAUTY));
        renderSettings.set("first_file", dialog->firstFilePath);
        renderSettings.set("animation_type",
            EnumWrapper(dialog->doSequence ? AnimationType::FILE_SEQUENCE : AnimationType::SINGLE_FRAME));

        SharedPtr<JobNode> fileNode =
            addNode(makeNode<LoadFileJob>(dialog->firstFilePath))->node->sharedFromThis();
        fileNode->connect(renderNode, "particles");

        SharedPtr<JobNode> cameraNode;
        switch (dialog->selectedCamera) {
        case CameraEnum::PERSPECTIVE: {
            cameraNode = addNode(makeNode<PerspectiveCameraJob>("Camera"))->node->sharedFromThis();
            VirtualSettings cameraSettings = cameraNode->getSettings();
            cameraSettings.set(GuiSettingsId::CAMERA_POSITION, Vector(0, 0, 2.5_f * scale));
            break;
        }
        case CameraEnum::ORTHO: {
            cameraNode = addNode(makeNode<OrthoCameraJob>("Camera"))->node->sharedFromThis();
            VirtualSettings cameraSettings = cameraNode->getSettings();
            cameraSettings.set(GuiSettingsId::CAMERA_ORTHO_FOV, 2 * scale);
            break;
        }
        case CameraEnum::FISHEYE:
            cameraNode = addNode(makeNode<FisheyeCameraJob>("Camera"))->node->sharedFromThis();
            break;
        default:
            NOT_IMPLEMENTED;
        }
        cameraNode->connect(renderNode, "camera");

        layoutNodes(*renderNode, Pixel(800, 200) - editor->offset());
        if (dialog->doRender) {
            callbacks->startRender(renderNode, globals, renderNode->instanceName());
        }
    }
    dialog->Destroy();
}

void NodeManager::selectRun() {
    SharedPtr<JobNode> node = activeRun.lock();
    if (node) {
        callbacks->startRun(node, globals, node->instanceName());
        return;
    }

    Array<SharedPtr<JobNode>> nodeList = getRootNodes();
    if (nodeList.empty()) {
        messageBox(
            "No simulation nodes added. First, create a simulation by double-clicking "
            "an item in the node list on the right side.",
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
            activeRun = node;
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

    // needed by MSVC
    this->SetBackgroundStyle(wxBG_STYLE_PAINT);

    this->Connect(wxEVT_PAINT, wxPaintEventHandler(NodeEditor::onPaint));
    this->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(NodeEditor::onMouseWheel));
    this->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(NodeEditor::onLeftDown));
    this->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(NodeEditor::onLeftUp));
    this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(NodeEditor::onRightUp));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(NodeEditor::onMouseMotion));
    this->Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(NodeEditor::onDoubleClick));

    this->Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&) {});
}

static void drawCenteredText(wxGraphicsContext* gc, const String& text, const Pixel from, const Pixel to) {
    wxDouble width, height, descent, externalLeading;
    gc->GetTextExtent(text.toUnicode(), &width, &height, &descent, &externalLeading);
    const Pixel pivot = (from + to) / 2 - Pixel(int(width), int(height)) / 2;
    gc->DrawText(text.toUnicode(), pivot.x, pivot.y);
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

    wxPen& imagePen = pens.insert(GuiJobType::IMAGE, *wxBLACK_PEN);
    imagePen.SetColour(wxColour(245, 245, 220));
    imagePen.SetStyle(wxPENSTYLE_SOLID);
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
    cameraPen.SetColour(wxColour(10, 80, 10));
    cameraPen.SetStyle(wxPENSTYLE_SHORT_DASH);

    wxPen& imagePen = pens.insert(GuiJobType::IMAGE, *wxBLACK_PEN);
    imagePen.SetColour(wxColour(131, 67, 51));
    imagePen.SetStyle(wxPENSTYLE_SOLID);
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
        const ExtJobType provided = to.vis->node->provides();
        return fromSlot.used && provided == fromSlot.type;
    } else {
        SPH_ASSERT(to.index != NodeSlot::RESULT_SLOT);
        const SlotData toSlot = to.vis->node->getSlot(to.index);
        const ExtJobType provided = from.vis->node->provides();
        return toSlot.used && provided == toSlot.type;
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
    const ExtJobType provided = vis.node->provides();
    // setup pen and brush
    const bool isLightTheme = background.intensity() > 0.5f;
    wxPen pen = getNodePen(provided, isLightTheme);
    wxBrush brush = *wxBLACK_BRUSH;
    Rgba brushColor;
    if (provided == JobType::PARTICLES) {
        brushColor = decreaseContrast(background, 0.1f, isLightTheme);
    } else {
        brushColor = background.blend(Rgba(pen.GetColour()), 0.2f);
    }
    brush.SetColour(wxColour(brushColor));
    gc->SetBrush(brush);
    gc->SetPen(pen);

    wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    font.SetPointSize(10);
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
    if (provided == JobType::PARTICLES) {
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
        gc->DrawText(slot.name.toUnicode(), p1.x + 14, p1.y - 10);
    }

    // result slot
    const Pixel resultSlot(position.x + size.x, position.y + FIRST_SLOT_Y);
    brush.SetColour(this->getSlotColor(NodeSlot{ &vis, NodeSlot::RESULT_SLOT }, background));
    gc->SetBrush(brush);

    pen = getNodePen(provided, isLightTheme);
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
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();

    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    if (!gc) {
        return;
    }
    // wxGraphicsContext::CreateMatrix behaves differently on wxGTK3, so let's do the transform by hand
    wxGraphicsMatrix matrix = gc->GetTransform();
    matrix.Translate(state.offset.x, state.offset.y);
    matrix.Scale(state.zoom, state.zoom);
    gc->SetTransform(matrix);

    const NodeMap& nodes = nodeMgr->getNodes();

    const Rgba background(dc.GetBackground().GetColour());

    // first layer - curves
    for (auto& element : nodes) {
        this->paintCurves(gc, background, element.value());
    }

    // second layer to paint over - nodes
    for (auto& element : nodes) {
        this->paintNode(gc, background, element.value());
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
    if (vis == nullptr) {
        // no node selected
        return;
    }
    const ExtJobType provided = vis->node->provides();
    if (provided == JobType::PARTICLES) {
        menu.Append(0, "Start");
    } else if (provided == GuiJobType::IMAGE) {
        menu.Append(1, "Render");
        menu.Append(2, "Preview");
    }


    menu.Append(3, "Clone");
    menu.Append(4, "Clone tree");
    menu.Append(5, "Layout");
    menu.Append(6, "Delete");
    menu.Append(7, "Delete tree");

    menu.Bind(wxEVT_COMMAND_MENU_SELECTED, [this, vis](wxCommandEvent& evt) {
        CHECK_FUNCTION(CheckFunction::NO_THROW);
        const Size index = evt.GetId();
        switch (index) {
        case 0:
            try {
                nodeMgr->startRun(*vis->node);
            } catch (const std::exception& e) {
                messageBox("Cannot run the node: " + exceptionMessage(e), "Error", wxOK);
            }
            break;
        case 1:
            try {
                nodeMgr->startRender(*vis->node);
            } catch (const std::exception& e) {
                messageBox("Cannot render the node: " + exceptionMessage(e), "Error", wxOK);
            }
            break;
        case 2:
            try {
                nodeWindow->createRenderPreview(*vis->node);
            } catch (const Exception& e) {
                messageBox("Cannot start render preview: " + exceptionMessage(e), "Error", wxOK);
            }
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
            nodeWindow->deleteNode(vis->node->sharedFromThis());
            break;
        case 7:
            nodeWindow->deleteTree(vis->node->sharedFromThis());
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
    virtual bool DoShowDialog(wxPropertyGrid* UNUSED(propGrid), wxPGProperty* UNUSED(property)) override {
        wxDirDialog dialog(nullptr, "Choose directory", "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
        if (dialog.ShowModal() == wxID_OK) {
            wxString path = dialog.GetPath();
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

    virtual bool DoShowDialog(wxPropertyGrid* UNUSED(propGrid), wxPGProperty* UNUSED(property)) override {
        Optional<Path> file = doSaveFileDialog("Save file...", formats.clone());
        if (file) {
            this->SetValue(wxString(file->string().toUnicode()));
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

    virtual bool DoShowDialog(wxPropertyGrid* UNUSED(propGrid), wxPGProperty* UNUSED(property)) override {
        Optional<Path> file = doOpenFileDialog("Open file...", formats.clone());
        if (file) {
            this->SetValue(wxString(file->string().toUnicode()));
            return true;
        } else {
            return false;
        }
    }
};

class FileProperty : public wxFileProperty {
    Function<wxPGEditorDialogAdapter*()> makeAdapter;

public:
    FileProperty(const String& label, const String& value)
        : wxFileProperty(label.toUnicode(), wxPG_LABEL, value.toUnicode()) {
        this->SetAttribute(wxPG_FILE_SHOW_FULL_PATH, true);
    }

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
        ComponentProperty(VectorProperty* parent, const String& name, const Vector& value, const Size index)
            : wxFloatProperty(name.toUnicode(), wxPG_LABEL, value[index])
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

        static StaticArray<String, 3> labels = { "X", "Y", "Z" };
        for (Size i = 0; i < 3; ++i) {
            components[i] = new ComponentProperty(this, labels[i], value, i);
            this->AppendChild(components[i]);
        }

        this->update(false);
    }

    Vector getVector() const {
        Vector v;
        for (Size i = 0; i < 3; ++i) {
            v[i] = Float(components[i]->GetValue().GetDouble());
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
        return Interval(components[0]->GetValue().GetDouble(), components[1]->GetValue().GetDouble());
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

    wxPGProperty* addCategory(const String& name) const {
        return grid->Append(new wxPropertyCategory(name.toUnicode()));
    }

    wxPGProperty* addBool(const String& name, const bool value) const {
        return grid->Append(new wxBoolProperty(name.toUnicode(), wxPG_LABEL, value));
    }

    wxPGProperty* addInt(const String& name, const int value) const {
        return grid->Append(new wxIntProperty(name.toUnicode(), wxPG_LABEL, value));
    }

    wxPGProperty* addFloat(const String& name, const Float value) const {
        return grid->Append(new wxFloatProperty(name.toUnicode(), wxPG_LABEL, value));
    }

    wxPGProperty* addVector(const String& name, const Vector& value) const {
        wxPGProperty* prop = grid->Append(new VectorProperty(grid, name.toUnicode(), value));
        grid->Collapse(prop);
        return prop;
    }

    wxPGProperty* addInterval(const String& name, const Interval& value) const {
        wxPGProperty* prop = grid->Append(new IntervalProperty(grid, name.toUnicode(), value));
        grid->Collapse(prop);
        return prop;
    }

    wxPGProperty* addString(const String& name, const String& value) const {
        return grid->Append(new wxStringProperty(name.toUnicode(), wxPG_LABEL, value.toUnicode()));
    }

    wxPGProperty* addPath(const String& name,
        const Path& value,
        const IVirtualEntry::PathType type,
        Array<IVirtualEntry::FileFormat>&& formats) const {
        FileProperty* prop = new FileProperty(name, value.string());
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

    wxPGProperty* addEnum(const String& name, const IVirtualEntry& entry) const {
        return addEnum<wxEnumProperty>(name, entry);
    }

    wxPGProperty* addFlags(const String& name, const IVirtualEntry& entry) const {
        return addEnum<wxFlagsProperty>(name, entry);
    }

    wxPGProperty* addExtra(const String& name, const ExtraEntry& extra, wxAuiManager* aui) const {
        IExtraEntry* entry = extra.getEntry().get();
        if (CurveEntry* curve = dynamic_cast<CurveEntry*>(entry)) {
            return grid->Append(new CurveProperty(name, curve->getCurve()));
        } else if (PaletteEntry* palette = dynamic_cast<PaletteEntry*>(entry)) {
            return grid->Append(new PaletteProperty(name, palette->getPalette(), aui));
        } else {
            NOT_IMPLEMENTED;
        }
    }

    void setTooltip(wxPGProperty* prop, const String& tooltip) const {
        grid->SetPropertyHelpString(prop, tooltip.toUnicode());
    }

private:
    template <typename TProperty>
    wxPGProperty* addEnum(const String& name, const IVirtualEntry& entry) const {
        wxArrayString values;
        wxArrayInt flags;
        EnumWrapper wrapper = entry.get();
        for (int id : EnumMap::getAll(wrapper.index)) {
            EnumWrapper option(id, wrapper.index);
            if (!entry.isValid(option)) {
                continue;
            }
            String rawName = EnumMap::toString(option.value, option.index);
            rawName.replaceAll("_", " ");
            values.Add(capitalize(rawName).toUnicode());
            flags.Add(option.value);
        }
        return grid->Append(new TProperty(name.toUnicode(), wxPG_LABEL, values, flags, wrapper.value));
    }
};


class AddParamsProc : public VirtualSettings::IEntryProc {
private:
    PropertyGrid wrapper;

    PropertyEntryMap& propertyEntryMap;
    wxAuiManager* aui;

public:
    AddParamsProc(wxPropertyGrid* grid, PropertyEntryMap& propertyEntryMapping, wxAuiManager* aui)
        : wrapper(grid)
        , propertyEntryMap(propertyEntryMapping)
        , aui(aui) {}

    virtual void onCategory(const String& name) const override {
        wrapper.addCategory(name);
    }

    virtual void onEntry(const String& UNUSED(key), IVirtualEntry& entry) const override {
        wxPGProperty* prop = nullptr;
        const String name = entry.getName();
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
            prop = wrapper.addExtra(name, entry.get(), aui);
            break;
        default:
            NOT_IMPLEMENTED;
        }

        const String tooltip = entry.getTooltip();
        if (!tooltip.empty()) {
            wrapper.setTooltip(prop, tooltip);
        }

        propertyEntryMap.insert(prop, &entry);

        SPH_ASSERT(propertyEntryMap[prop]->enabled() ||
                   propertyEntryMap[prop]->getType() != IVirtualEntry::Type(20)); // dummy call
    }
};

class JobTreeData : public wxTreeItemData {
    RawPtr<IJobDesc> desc;

public:
    explicit JobTreeData(RawPtr<IJobDesc> desc)
        : desc(desc) {}

    AutoPtr<IJob> create() const {
        return desc->create(NOTHING);
    }

    String tooltip() const {
        return desc->tooltip();
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
            entry->set(Float(value.GetDouble()));
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
            String stringValue(value.GetString().wc_str());
            /// \todo generalize, using some kind of validator
            if (entry->getName() == "Name") {
                UniqueNameManager nameMgr = nodeMgr->makeUniqueNameManager();
                stringValue = nameMgr.getName(stringValue);
            }
            entry->set(stringValue);
            break;
        }
        case IVirtualEntry::Type::PATH:
            entry->set(Path(String(value.GetString().wc_str())));
            break;
        case IVirtualEntry::Type::ENUM:
        case IVirtualEntry::Type::FLAGS: {
            EnumWrapper ew = entry->get();
            ew.value = value.GetLong();
            entry->set(ew);
            break;
        }
        case IVirtualEntry::Type::EXTRA: {
            if (CurveProperty* curve = dynamic_cast<CurveProperty*>(prop)) {
                ExtraEntry extra(makeAuto<CurveEntry>(curve->getCurve()));
                entry->set(extra);
            } else if (PaletteProperty* palette = dynamic_cast<PaletteProperty*>(prop)) {
                ExtraEntry extra(makeAuto<PaletteEntry>(palette->getPalette()));
                entry->set(extra);
            }
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


    TooltippedWindow<wxTreeCtrl, wxTreeItemId>* jobView = new TooltippedWindow<wxTreeCtrl, wxTreeItemId>(
        this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_DEFAULT_STYLE | wxTR_HIDE_ROOT);
    jobView->SetMinSize(wxSize(300, -1));

    wxTreeItemId rootId = jobView->AddRoot("Nodes");

    FlatMap<String, wxTreeItemId> categoryItemIdMap;
    for (const AutoPtr<IJobDesc>& desc : enumerateRegisteredJobs()) {
        const String& cat = desc->category();
        if (Optional<wxTreeItemId&> id = categoryItemIdMap.tryGet(cat)) {
            jobView->AppendItem(
                id.value(), desc->className().toUnicode(), -1, -1, new JobTreeData(desc.get()));
        } else {
            wxTreeItemId catId = jobView->AppendItem(rootId, cat.toUnicode());
            jobView->AppendItem(catId, desc->className().toUnicode(), -1, -1, new JobTreeData(desc.get()));
            categoryItemIdMap.insert(cat, catId);
        }
    }

    wxTreeItemId presetsId = jobView->AppendItem(rootId, "presets");
    std::map<wxTreeItemId, Presets::Id> presetsIdMap;
    for (Presets::Id id : EnumMap::getAll<Presets::Id>()) {
        String name = EnumMap::toString(id);
        name.replaceAll("_", " ");

        wxTreeItemId itemId = jobView->AppendItem(presetsId, name.toUnicode());
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
                    wxRect rect;
                    jobView->GetBoundingRect(id, rect);
                    String text = setLineBreak(data->tooltip(), 50);
                    jobView->showTooltip(pos, rect, id, text);

                    nodeEditor->invalidateMousePosition();
                });
            }
        } else {
            callback.stop();
        }

        jobView->hideTooltipsIfOutsideRect(pos);
    });

    jobView->Bind(wxEVT_LEAVE_WINDOW, [jobView](wxMouseEvent& evt) {
        wxPoint pos = evt.GetPosition();
        jobView->hideTooltipsIfOutsideRect(pos);
    });

    jobView->Bind(wxEVT_KILL_FOCUS, [jobView](wxFocusEvent& evt) {
        jobView->hideTooltips();
        evt.Skip();
    });

    jobView->Bind(wxEVT_TREE_ITEM_ACTIVATED, [=](wxTreeEvent& evt) {
        wxTreeItemId id = evt.GetItem();
        UniqueNameManager nameMgr = nodeMgr->makeUniqueNameManager();
        if (presetsIdMap.find(id) != presetsIdMap.end()) {
            const Presets::Id presetId = presetsIdMap.at(id);
            SharedPtr<JobNode> presetNode = Presets::make(presetId, nameMgr);
            nodeMgr->addNodes(*presetNode);

            // hack to set default particle radius to 0.35 for SPH sims
            static FlatSet<Presets::Id> SPH_SIMS = FlatSet<Presets::Id>(ElementsUniqueTag{},
                {
                    Presets::Id::COLLISION,
                    Presets::Id::CRATERING,
                    Presets::Id::PLANETESIMAL_MERGING,
                    Presets::Id::ACCRETION_DISK,
                });
            static bool defaultSet = false;
            if (!defaultSet && SPH_SIMS.contains(presetId)) {
                defaultSet = true;
                GuiSettings& gui = Project::getInstance().getGuiSettings();
                gui.set(GuiSettingsId::PARTICLE_RADIUS, 0.35_f);
            }
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
                    const Optional<IoEnum> type = getIoEnum(path->extension().string());
                    if (type) {
                        settings.set(RunSettingsId::RUN_OUTPUT_TYPE, EnumWrapper(type.value()));
                    } else {
                        messageBox(
                            "Unknown file extension '" + path->extension().string() + "'", "Error", wxOK);
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

    // PalettePane* palettePane = new PalettePane(this, project, &*aui);

    /*wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(grid, 1, wxEXPAND | wxLEFT);
    sizer->Add(mainPanel, 3, wxEXPAND | wxALL);
    sizer->Add(jobView, 1, wxEXPAND | wxRIGHT);
    this->SetSizerAndFit(sizer);*/
    this->SetAutoLayout(true);

    wxAuiPaneInfo info;
    info.Name("PropertyGrid").Left().MinSize(wxSize(300, -1));
    aui->AddPane(grid, info);

    info.Name("Editor").Center();
    aui->AddPane(nodeEditor, info);

    info.Name("JobView").Right();
    aui->AddPane(jobView, info);

    // info.Name("PalettePane").Right().Hide();
    // aui->AddPane(palettePane, info);
    aui->Update();

    panelInfoMap.insert(ID_LIST, &aui->GetPane(jobView));
    panelInfoMap.insert(ID_PROPERTIES, &aui->GetPane(grid));
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

    displayedNode = node.sharedFromThis();
    settings = node.getSettings();
    this->updateProperties();
}

void NodeWindow::clearGrid() {
    grid->CommitChangesFromEditor();
    grid->Clear();
    displayedNode = nullptr;
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

void NodeWindow::selectRender() {
    nodeMgr->selectRender();
}

void NodeWindow::renderSetup() {
    nodeMgr->renderSetup();
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

    SharedPtr<ConfigNode> layoutNode = config.addNode("layout");
    String data(aui->SavePerspective().wc_str());
    layoutNode->set("perspective", data);
}

void NodeWindow::load(Config& config) {
    nodeMgr->load(config);
    nodeEditor->load(config);

    SharedPtr<ConfigNode> layoutNode = config.tryGetNode("layout");
    if (layoutNode) {
        String data = layoutNode->get<String>("perspective");
        aui->LoadPerspective(data.toUnicode());
    }
}

void NodeWindow::addNode(const SharedPtr<JobNode>& node) {
    nodeMgr->addNode(node);
}

void NodeWindow::addNodes(JobNode& node) {
    nodeMgr->addNodes(node);
}

void NodeWindow::deleteNode(const SharedPtr<JobNode>& node) {
    if (displayedNode == node) {
        this->clearGrid();
    }
    nodeMgr->deleteNode(*node);
}

void NodeWindow::deleteTree(const SharedPtr<JobNode>& node) {
    node->enumerate([this](const SharedPtr<JobNode>& child) {
        if (displayedNode == child) {
            this->clearGrid();
        }
    });
    nodeMgr->deleteTree(*node);
}

SharedPtr<JobNode> NodeWindow::createNode(AutoPtr<IJob>&& job) {
    SharedPtr<JobNode> node = makeShared<JobNode>(std::move(job));
    nodeMgr->addNode(node);
    return node;
}

void NodeWindow::createRenderPreview(JobNode& node) {
    renderPane = nodeMgr->createRenderPreview(this, node);
    wxAuiPaneInfo info;
    info.Name("Preview")
        .Right()
        .MinSize(wxSize(300, 300))
        .CaptionVisible(true)
        .DockFixed(false)
        .CloseButton(true)
        .Caption("Preview")
        .DestroyOnClose();
    aui->AddPane(renderPane, info);

    aui->Update();
}

void NodeWindow::updateProperties() {
    const wxString states = grid->SaveEditableState(wxPropertyGrid::ScrollPosState);
    grid->Clear();
    propertyEntryMap.clear();

    try {
        AddParamsProc proc(grid, propertyEntryMap, &*aui);
        settings.enumerate(proc);
    } catch (const Exception& e) {
        SPH_ASSERT(false, e.what());
        MARK_USED(e);
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
