#pragma once

/// \file RunNode.h
/// \brief
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "io/Output.h"
#include "objects/containers/UnorderedMap.h"
#include "objects/wrappers/SharedPtr.h"
#include "quantities/Storage.h"
#include "run/IRun.h"
#include "run/VirtualSettings.h"
#include "run/Worker.h"
#include "system/Statistics.h"
#include <set>

NAMESPACE_SPH_BEGIN

class ConfigNode;
class WorkerNode;

/// \brief Interface used during worker evaluation.
class IWorkerCallbacks : public IRunCallbacks {
public:
    /// \brief Notifies the caller that a new worker started running.
    virtual void onStart(const IWorker& worker) = 0;

    /// \brief Notifies the caller that the current worker ended.
    ///
    /// If the worker was a particle worker, it passes its storage and run statistics as parameters, otherwise
    /// the storage is passed as empty.
    virtual void onEnd(const Storage& storage, const Statistics& stats) = 0;
};

class NullWorkerCallbacks : public IWorkerCallbacks {
public:
    virtual void onStart(const IWorker& UNUSED(worker)) override {}

    virtual void onEnd(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {}

    virtual void onSetUp(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

    virtual void onTimeStep(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

    virtual bool shouldAbortRun() const override {
        return false;
    }
};

struct SlotData {
    /// \brief Identifier of the slot, used by the worker to obtain the provided data.
    std::string name;

    /// \brief Specifies the type of the slot, or the type of the node connecting to it.
    WorkerType type;

    /// \brief Whether the node is used by the worker.
    ///
    /// Worker can enable or disable their slots, depending on their internal state, for example the slot for
    /// custom shape of \ref MonolithicBodyIc is enabled only if the associated flag parameter is set to true.
    bool used;

    /// \brief Node currently connected to the slot.
    ///
    /// May be nullptr if no node is connected.
    SharedPtr<WorkerNode> provider;
};

/// \brief Building block of a simulation hierarchy.
///
/// Each node can have any number of providers (preconditions of the worker).
class WorkerNode : public ShareFromThis<WorkerNode> {
    /// Maps slot names to connected providers
    UnorderedMap<std::string, SharedPtr<WorkerNode>> providers;

    /// All dependent nodes (i.e. nodes that have this node as provider) in no particular order.
    Array<WeakPtr<WorkerNode>> dependents;

    /// Worker object of this node
    SharedPtr<IWorker> worker;

public:
    /// \brief Creates a new node, given a worker object.
    WorkerNode(AutoPtr<IWorker>&& worker);

    /// \brief Returns the class name of the worker.
    std::string className() const;

    /// \brief Returns the instance name of the worker.
    std::string instanceName() const;

    /// \brief Returns settings object allowing to access and modify the state of the worker.
    VirtualSettings getSettings() const;

    /// \brief Returns the type of the worker.
    WorkerType provides() const;

    /// \brief Connects this node to given dependent node.
    ///
    /// \param dependent Dependent node to which this node is connected.
    /// \param slotName Name of the slot of the dependent node.
    /// \throw InvalidSetup if no such slot exists or it has a different type.
    void connect(SharedPtr<WorkerNode> dependent, const std::string& slotName);

    /// \brief Disconnects this node from given dependent node.
    ///
    /// \param dependent Dependent node to be disconnected.
    /// \throw InvalidSetup if the given node is not a dependent.
    void disconnect(SharedPtr<WorkerNode> dependent);

    /// \brief Disconnects all dependent nodes from this node.
    void disconnectAll();

    /// \brief Returns the number of provider slots of this node.
    Size getSlotCnt() const;

    /// \brief Returns the information about given slot.
    SlotData getSlot(const Size index) const;

    /// \brief Returns the number of dependent nodes.
    Size getDependentCnt() const;

    /// \brief Returns a dependent node with given index.
    SharedPtr<WorkerNode> getDependent(const Size index) const;

    /// \brief Enumerates all nodes in the hierarchy.
    ///
    /// Function call provided function for this node and recursively for all providers of this node. Each
    /// node is visited only once.
    void enumerate(Function<void(SharedPtr<WorkerNode> worker, Size depth)> func);

    /// \brief Evaluates the node and all its providers.
    ///
    /// The worker of this node its evaluated after all the providers finished and all inputs of the worker
    /// have been set up.
    /// \param global Global settings, used by all nodes in the hierarchy.
    /// \param callbacks Interface allowing to get a feedback from evaluated nodes, see \ref IWorkerCallbacks.
    void run(const RunSettings& global, IWorkerCallbacks& callbacks);

private:
    void enumerate(Function<void(SharedPtr<WorkerNode> worker, Size depth)> func,
        Size depth,
        std::set<WorkerNode*>& visited);

    void run(const RunSettings& global, IWorkerCallbacks& callbacks, std::set<WorkerNode*>& visited);
};

template <typename TWorker, typename... TArgs>
SharedPtr<WorkerNode> makeNode(TArgs&&... args) {
    return makeShared<WorkerNode>(makeAuto<TWorker>(std::forward<TArgs>(args)...));
}


NAMESPACE_SPH_END
