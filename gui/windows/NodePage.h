#pragma once

#include "gui/objects/Color.h"
#include "gui/objects/Point.h"
#include "gui/windows/BatchDialog.h"
#include "objects/containers/UnorderedMap.h"
#include "run/Node.h"
#include <map>
#include <wx/panel.h>

class wxPropertyGrid;
class wxPGProperty;
class wxGraphicsContext;
class wxAuiManager;
class wxAuiPaneInfo;

NAMESPACE_SPH_BEGIN

class NodeEditor;
class Config;

struct VisNode {
    RawPtr<JobNode> node;
    Pixel position;

    VisNode() = default;

    VisNode(RawPtr<JobNode> node, Pixel position)
        : node(node)
        , position(position) {}

    static constexpr Size SIZE_X = 160;

    Pixel size() const {
        return Pixel(SIZE_X, max<int>(60 + node->getSlotCnt() * 25, 80));
    }
};

struct NodeSlot {
    RawPtr<const VisNode> vis;
    Size index = 0;

    static constexpr Size RESULT_SLOT = -1;

    Pixel position() const;

    bool operator==(const NodeSlot& other) const {
        return vis == other.vis && index == other.index;
    }
    bool operator!=(const NodeSlot& other) const {
        return !(*this == other);
    }
};

using NodeMap = UnorderedMap<SharedPtr<JobNode>, VisNode>;

class INodeManagerCallbacks : public Polymorphic {
public:
    virtual void startRun(SharedPtr<JobNode> node, const RunSettings& settings) const = 0;

    virtual void markUnsaved() const = 0;
};

class NodeManager {
private:
    NodeEditor* editor;

    BatchManager batch;

    NodeMap nodes;

    RunSettings globals = EMPTY_SETTINGS;

    SharedPtr<INodeManagerCallbacks> callbacks;

    WeakPtr<JobNode> activeNode;

public:
    NodeManager(NodeEditor* editor, SharedPtr<INodeManagerCallbacks> callbacks);

    VisNode* addNode(const SharedPtr<JobNode>& node);

    VisNode* addNode(const SharedPtr<JobNode>& node, const Pixel position);

    void addNodes(JobNode& node);

    void cloneHierarchy(JobNode& node);

    void layoutNodes(JobNode& node, const Pixel position);

    const NodeMap& getNodes() const {
        return nodes;
    }

    void deleteNode(JobNode& node);

    void deleteTree(JobNode& node);

    void deleteAll();

    VisNode* getSelectedNode(const Pixel position);

    NodeSlot getSlotAtPosition(const Pixel position);

    void save(Config& config);

    void load(Config& config);

    void startRun(JobNode& node);

    void startBatch(JobNode& node);

    void startAll();

    Array<SharedPtr<JobNode>> getTopLevelNodes() const;

    VirtualSettings getGlobalSettings();

    void showBatchDialog();

    void selectRun();

    UniqueNameManager makeUniqueNameManager() const;
};

class NodeWindow;

class NodeEditor : public wxPanel {
private:
    SharedPtr<NodeManager> nodeMgr;

    SharedPtr<INodeManagerCallbacks> callbacks;

    NodeWindow* nodeWindow;

    struct {
        /// Translation of the panel.
        Pixel offset = Pixel(0, 0);

        /// Zoom of the panel.
        float zoom = 1.f;

        Pixel mousePosition = Pixel(0, 0);

        /// Node currently selected by mouse (clicked, dragged, etc.)
        VisNode* selected = nullptr;

        /// Last double-clicked node.
        VisNode* activated = nullptr;

        /// Source slot when connecting
        Optional<NodeSlot> connectingSlot;

        NodeSlot lastSlot;

        Pixel mouseDownPos = Pixel(0, 0);

    } state;

public:
    NodeEditor(NodeWindow* parent, SharedPtr<INodeManagerCallbacks> callbacks);

    void setNodeMgr(SharedPtr<NodeManager> mgr) {
        nodeMgr = mgr;
    }

    Pixel offset() const {
        return state.offset;
    }

    Pixel transform(const Pixel position) const {
        return (position - state.offset) / state.zoom;
    }

    void activate(VisNode* vis) {
        state.activated = vis;
    }

    void save(Config& config);

    void load(Config& config);

private:
    void paintCurves(wxGraphicsContext* gc, const Rgba& background, const VisNode& node);

    void paintNode(wxGraphicsContext* gc, const Rgba& background, const VisNode& node);

    wxColour getSlotColor(const NodeSlot& slot, const Rgba& background);

    void onPaint(wxPaintEvent& evt);

    void onMouseMotion(wxMouseEvent& evt);

    void onMouseWheel(wxMouseEvent& evt);

    void onLeftDown(wxMouseEvent& evt);

    void onLeftUp(wxMouseEvent& evt);

    void onRightUp(wxMouseEvent& evt);

    void onDoubleClick(wxMouseEvent& evt);
};

using PropertyEntryMap = FlatMap<wxPGProperty*, IVirtualEntry*>;

class NodeWindow : public wxPanel {
public:
    enum PanelId {
        ID_PROPERTIES,
        ID_LIST,
    };

private:
    SharedPtr<NodeManager> nodeMgr;

    /// Settings associated with currently displayed properties
    VirtualSettings settings;

    NodeEditor* nodeEditor;

    /// Maps properties in grid to entries in settings
    PropertyEntryMap propertyEntryMap;

    wxPropertyGrid* grid;
    AutoPtr<wxAuiManager> aui;

    FlatMap<PanelId, wxAuiPaneInfo*> panelInfoMap;

public:
    NodeWindow(wxWindow* parent, SharedPtr<INodeManagerCallbacks> callbacks);

    ~NodeWindow();

    void showPanel(const PanelId id);

    void selectNode(const JobNode& node);

    void clearGrid();

    void showGlobals();

    void showBatchDialog();

    void selectRun();

    void save(Config& config);

    void load(Config& config);

    SharedPtr<JobNode> addNode(AutoPtr<IJob>&& worker);

    void reset();

private:
    void updateProperties();

    void updateEnabled(wxPropertyGrid* grid);
};

NAMESPACE_SPH_END
