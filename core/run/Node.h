#pragma once

/// \file RunNode.h
/// \brief Wrapper of IJob with connections to dependents and providers.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "io/Output.h"
#include "objects/containers/CallbackSet.h"
#include "objects/containers/UnorderedMap.h"
#include "objects/wrappers/Any.h"
#include "quantities/Storage.h"
#include "run/IRun.h"
#include "run/Job.h"
#include "run/VirtualSettings.h"
#include "system/Statistics.h"
#include <set>

NAMESPACE_SPH_BEGIN

class ConfigNode;
class JobNode;

/// \brief Interface used during job evaluation.
class IJobCallbacks : public IRunCallbacks {
public:
    /// \brief Notifies the caller that a new job started running.
    virtual void onStart(const IJob& job) = 0;

    /// \brief Notifies the caller that the current job ended.
    ///
    /// If the job was a particle job, it passes its storage and run statistics as parameters, otherwise
    /// the storage is passed as empty.
    virtual void onEnd(const Storage& storage, const Statistics& stats) = 0;
};

class NullJobCallbacks : public IJobCallbacks {
public:
    virtual void onStart(const IJob& UNUSED(job)) override {}

    virtual void onEnd(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {}

    virtual void onSetUp(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

    virtual void onTimeStep(const Storage& UNUSED(storage), Statistics& UNUSED(stats)) override {}

    virtual bool shouldAbortRun() const override {
        return false;
    }
};

/// \brief Provides an interface for running a simulation.
class INode : public Polymorphic {
public:
    virtual void run(const RunSettings& global, IJobCallbacks& callbacks) = 0;
};

struct SlotData {
    /// \brief Identifier of the slot, used by the job to obtain the provided data.
    std::string name;

    /// \brief Specifies the type of the slot, or the type of the node connecting to it.
    ExtJobType type;

    /// \brief Whether the node is used by the job.
    ///
    /// job can enable or disable their slots, depending on their internal state, for example the slot for
    /// custom shape of \ref MonolithicBodyIc is enabled only if the associated flag parameter is set to true.
    bool used;

    /// \brief Node currently connected to the slot.
    ///
    /// May be nullptr if no node is connected.
    SharedPtr<JobNode> provider;
};

enum JobNotificationType {
    ENTRY_CHANGED,
    PROVIDER_CONNECTED,
    PROVIDER_DISCONNECTED,
    DEPENDENT_CONNECTED,
    DEPENDENT_DISCONNECTED,
};

/// \brief Building block of a simulation hierarchy.
///
/// Each node can have any number of providers (preconditions of the job).
class JobNode : public ShareFromThis<JobNode>, public INode {
public:
    using Accessor = Function<void(JobNotificationType, const Any& value)>;

private:
    /// Maps slot names to connected providers
    UnorderedMap<std::string, SharedPtr<JobNode>> providers;

    /// All dependent nodes (i.e. nodes that have this node as provider) in no particular order.
    Array<WeakPtr<JobNode>> dependents;

    /// Job object of this node
    AutoPtr<IJob> job;

    /// Set of callbacks called when a node property is changed.
    CallbackSet<Accessor> accessors;

public:
    /// \brief Creates a new node, given a job object.
    explicit JobNode(AutoPtr<IJob>&& job);

    /// \brief Returns the instance name of the job.
    std::string instanceName() const;

    /// \brief Returns the class name of the job.
    std::string className() const;

    /// \brief Returns settings object allowing to access and modify the state of the job.
    VirtualSettings getSettings() const;

    /// \brief Returns the underlying job.
    RawPtr<IJob> getJob() const;

    /// \brief Adds an accessor for entries returned by the \ref getSettings function.
    ///
    /// The accessors are shared among all \ref VirtualSettings objects obtained from this node. When a new
    /// accessor is added, existing entries will start using it as well.
    void addAccessor(const SharedToken& owner, const Accessor& accessor);

    /// \brief Returns the type of the job.
    ExtJobType provides() const;

    /// \brief Connects this node to given dependent node.
    ///
    /// \param dependent Dependent node to which this node is connected.
    /// \param slotName Name of the slot of the dependent node.
    /// \throw InvalidSetup if no such slot exists or it has a different type.
    void connect(SharedPtr<JobNode> dependent, const std::string& slotName);

    /// \brief Disconnects this node from given dependent node.
    ///
    /// \param dependent Dependent node to be disconnected.
    /// \throw InvalidSetup if the given node is not a dependent.
    void disconnect(SharedPtr<JobNode> dependent);

    /// \brief Disconnects all dependent nodes from this node.
    void disconnectAll();

    /// \brief Returns the number of provider slots of this node.
    Size getSlotCnt() const;

    /// \brief Returns the information about given slot.
    SlotData getSlot(const Size index) const;

    /// \brief Returns the number of dependent nodes.
    Size getDependentCnt() const;

    /// \brief Returns a dependent node with given index.
    SharedPtr<JobNode> getDependent(const Size index) const;

    /// \brief Enumerates all nodes in the hierarchy.
    ///
    /// Function call provided function for this node and recursively for all providers of this node. Each
    /// node is visited only once.
    void enumerate(Function<void(const SharedPtr<JobNode>& job)> func);

    /// \copydoc enumerate
    ///
    /// The function is given the visited job and its depth in the hierarchy.
    void enumerate(Function<void(const SharedPtr<JobNode>& job, Size depth)> func);

    /// \brief Evaluates the node and all its providers.
    ///
    /// The job is evaluated after all the providers finished and all inputs of the job have been set up.
    /// \param global Global settings, used by all nodes in the hierarchy.
    /// \param callbacks Interface allowing to get a feedback from evaluated nodes, see \ref IJobCallbacks.
    virtual void run(const RunSettings& global, IJobCallbacks& callbacks) override;

    /// \brief Evaluates all provides, without executing the node itself.
    ///
    /// This functions does not have to be called before \ref run, as it is called from within \ref run.
    /// It is useful for manual execution of jobs.
    virtual void prepare(const RunSettings& global, IJobCallbacks& callbacks);

private:
    void enumerate(Function<void(const SharedPtr<JobNode>& job, Size depth)> func,
        Size depth,
        std::set<JobNode*>& visited);

    void run(const RunSettings& global, IJobCallbacks& callbacks, std::set<JobNode*>& visited);

    void prepare(const RunSettings& global, IJobCallbacks& callbacks, std::set<JobNode*>& visited);
};

/// \brief Helper function for creating job nodes.
template <typename TJob, typename... TArgs>
SharedPtr<JobNode> makeNode(TArgs&&... args) {
    return makeShared<JobNode>(makeAuto<TJob>(std::forward<TArgs>(args)...));
}

/// \brief Clones a single node.
///
/// No slots of the returned node are connected.
/// \param node Node to clone.
/// \param name Instance name of the returned clone. Empty means the name is autogenerated.
AutoPtr<JobNode> cloneNode(const JobNode& node, const std::string& name = "");

/// \brief Clones all nodes in the hierarchy.
///
/// Returned node is already connected to the other cloned nodes.
/// \param node Root node of the cloned hierarchy
/// \param prefix Prefix added to all names of cloned nodes. NOTHING means the cloned names are autogenerated.
SharedPtr<JobNode> cloneHierarchy(JobNode& node, const Optional<std::string>& prefix = NOTHING);

NAMESPACE_SPH_END
