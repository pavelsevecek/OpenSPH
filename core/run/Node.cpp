#include "run/Node.h"
#include "quantities/Quantity.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

JobNode::JobNode(AutoPtr<IJob>&& job)
    : job(std::move(job)) {}

std::string JobNode::className() const {
    return job->className();
}

std::string JobNode::instanceName() const {
    return job->instanceName();
}

class SetAccessorsProc : public VirtualSettings::IEntryProc {
private:
    SharedPtr<JobNode> node;
    CallbackSet<JobNode::Accessor> callbacks;

public:
    explicit SetAccessorsProc(const SharedPtr<JobNode>& node, const CallbackSet<JobNode::Accessor>& callbacks)
        : node(node)
        , callbacks(callbacks) {}

    virtual void onCategory(const std::string& UNUSED(name)) const override {}

    virtual void onEntry(const std::string& key, IVirtualEntry& entry) const override {
        EntryControl* control = dynamic_cast<EntryControl*>(&entry);
        SPH_ASSERT(control);
        if (control) {
            control->addAccessor(node, [key, f = callbacks](const IVirtualEntry::Value& UNUSED(value)) {
                f(JobNotificationType::ENTRY_CHANGED, key);
            });
        }
    }
};

VirtualSettings JobNode::getSettings() const {
    VirtualSettings settings = job->getSettings();
    SetAccessorsProc proc(this->sharedFromThis(), accessors);
    settings.enumerate(proc);
    return settings;
}

RawPtr<IJob> JobNode::getJob() const {
    return job.get();
}

void JobNode::addAccessor(const SharedToken& owner, const Accessor& accessor) {
    accessors.insert(owner, accessor);
}

ExtJobType JobNode::provides() const {
    return job->provides();
}

void JobNode::connect(SharedPtr<JobNode> node, const std::string& slotName) {
    UnorderedMap<std::string, ExtJobType> slots = node->job->getSlots();
    if (slots.contains(slotName)) {
        const ExtJobType provided = job->provides();
        if (provided != slots[slotName]) {
            throw InvalidSetup("Cannot connect node '" + job->instanceName() + "' to slot '" + slotName +
                               "' of node '" + node->instanceName() +
                               "', the slot expects different type of node.");
        }

        node->providers.insert(slotName, this->sharedFromThis());
        dependents.push(node);

        accessors(JobNotificationType::DEPENDENT_CONNECTED, node);
        node->accessors(JobNotificationType::PROVIDER_CONNECTED, sharedFromThis());

    } else {
        std::string list;
        for (const auto& element : slots) {
            list += element.key + ", ";
        }
        throw InvalidSetup("Invalid slot '" + slotName + "' for node '" + node->instanceName() +
                           "'.\nShould be one of: " + list);
    }
}


void JobNode::disconnect(SharedPtr<JobNode> dependent) {
    bool dependentRemoved = false;
    // remove the dependent from the list of dependents
    for (Size i = 0; i < dependents.size(); ++i) {
        if (dependents[i].lock() == dependent) {
            dependents.remove(i);
            dependentRemoved = true;
            break;
        }
    }
    if (!dependentRemoved) {
        throw InvalidSetup(
            "Node '" + dependent->instanceName() + "' to be disconnected is not a dependent node.");
    }


    // remove us from their list of providers
    SharedPtr<JobNode> self = this->sharedFromThis();
    bool providerRemoved = false;
    for (auto& element : dependent->providers) {
        if (element.value == self) {
            dependent->providers.remove(element.key);
            providerRemoved = true;
            break;
        }
    }
    if (!providerRemoved) {
        throw InvalidSetup("Node '" + dependent->instanceName() +
                           "' to be disconnected does not list node '" + this->instanceName() +
                           "' as a provider");
    }

    accessors(JobNotificationType::DEPENDENT_DISCONNECTED, dependent);
    dependent->accessors(JobNotificationType::PROVIDER_DISCONNECTED, sharedFromThis());
}

void JobNode::disconnectAll() {
    while (!dependents.empty()) {
        this->disconnect(dependents.back().lock());
    }
}

Size JobNode::getSlotCnt() const {
    return job->getSlots().size();
}

SlotData JobNode::getSlot(const Size index) const {
    const UnorderedMap<std::string, ExtJobType> slots = job->getSlots();
    const UnorderedMap<std::string, ExtJobType> required = job->requires();
    if (index >= slots.size()) {
        throw InvalidSetup("Cannot query slot #" + std::to_string(index) + ", node '" + job->instanceName() +
                           "' has only " + std::to_string(slots.size()) + " slots");
    }

    const auto iter = slots.begin() + index;
    const bool used = required.contains(iter->key);

    SharedPtr<JobNode> provider;
    if (auto optProvider = providers.tryGet(iter->key)) {
        provider = optProvider.value();
    }
    return { iter->key, iter->value, used, provider };
}

Size JobNode::getDependentCnt() const {
    return dependents.size();
}

SharedPtr<JobNode> JobNode::getDependent(const Size index) const {
    return dependents[index].lock();
}

void JobNode::enumerate(Function<void(const SharedPtr<JobNode>&)> func) {
    this->enumerate([func](const SharedPtr<JobNode>& node, Size UNUSED(depth)) { func(node); });
}

void JobNode::enumerate(Function<void(const SharedPtr<JobNode>&, Size)> func) {
    std::set<JobNode*> visited;
    this->enumerate(func, 0, visited);
}

void JobNode::enumerate(Function<void(const SharedPtr<JobNode>& job, Size depth)> func,
    Size depth,
    std::set<JobNode*>& visited) {
    auto pair = visited.insert(this);
    if (!pair.second) {
        return;
    }
    func(this->sharedFromThis(), depth);
    for (auto& element : providers) {
        element.value->enumerate(func, depth + 1, visited);
    }
}

void JobNode::run(const RunSettings& global, IJobCallbacks& callbacks) {
    std::set<JobNode*> visited;
    this->run(global, callbacks, visited);
}

void JobNode::prepare(const RunSettings& global, IJobCallbacks& callbacks) {
    std::set<JobNode*> visited;
    this->prepare(global, callbacks, visited);
}

void JobNode::prepare(const RunSettings& global, IJobCallbacks& callbacks, std::set<JobNode*>& visited) {
    // first, run all dependencies
    for (auto& element : providers) {
        if (!job->requires().contains(element.key)) {
            // job may change its requirements during (or before) the run, in this case it's not a real
            // dependency
            continue;
        }
        SharedPtr<JobNode> provider = element.value;
        if (visited.find(&*provider) == visited.end()) {
            visited.insert(&*provider);
            provider->run(global, callbacks, visited);
        }

        JobContext result = provider->job->getResult();
        if (provider->getDependentCnt() > 1) {
            // dependents modify the result in place, so we need to clone it
            result = result.clone();
        }
        job->inputs.insert(element.key, result);
    }
}

void JobNode::run(const RunSettings& global, IJobCallbacks& callbacks, std::set<JobNode*>& visited) {
    this->prepare(global, callbacks, visited);

    if (callbacks.shouldAbortRun()) {
        return;
    }

    callbacks.onStart(*job);
    job->evaluate(global, callbacks);

    JobContext result = job->getResult();
    if (SharedPtr<ParticleData> data = result.tryGetValue<ParticleData>()) {
        callbacks.onEnd(data->storage, data->stats);
    } else {
        callbacks.onEnd(Storage(), Statistics());
    }

    // release memory of providers
    for (auto& element : providers) {
        SharedPtr<JobNode> provider = element.value;
        provider->job->getResult().release();
    }
}

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

static std::string clonedName(const std::string& name) {
    const std::size_t n1 = name.find_last_of('(');
    const std::size_t n2 = name.find_last_of(')');
    if (n1 != std::string::npos && n2 != std::string::npos && n1 < n2) {
        const std::string numStr = name.substr(n1 + 1, n2 - n1 - 1);
        const int num = stoi(numStr);
        return name.substr(0, n1) + " (" + std::to_string(num + 1) + ")";
    } else {
        return name + " (1)";
    }
}

AutoPtr<JobNode> cloneNode(const JobNode& node, const std::string& name) {
    RawPtr<IJobDesc> desc = getJobDesc(node.className());
    SPH_ASSERT(desc);

    AutoPtr<IJob> job = desc->create(name.empty() ? clonedName(node.instanceName()) : name);
    VirtualSettings target = job->getSettings();
    VirtualSettings source = node.getSettings();
    CopyEntriesProc proc(target);
    source.enumerate(proc);

    return makeAuto<JobNode>(std::move(job));
}

SharedPtr<JobNode> cloneHierarchy(JobNode& node, const Optional<std::string>& prefix) {
    // maps original node to cloned nodes
    FlatMap<SharedPtr<JobNode>, SharedPtr<JobNode>> nodeMap;

    // first, clone all nodes and build up the map
    node.enumerate([&nodeMap, &prefix](SharedPtr<JobNode> node, Size UNUSED(depth)) {
        std::string name;
        if (!prefix) {
            name = clonedName(node->instanceName());
        } else {
            name = prefix.value() + node->instanceName();
        }
        SharedPtr<JobNode> clonedNode = cloneNode(*node, name);
        nodeMap.insert(node, clonedNode);
    });

    // second, connect cloned nodes to get the same hierarchy
    node.enumerate([&nodeMap](SharedPtr<JobNode> node, Size UNUSED(depth)) {
        for (Size i = 0; i < node->getSlotCnt(); ++i) {
            SlotData slot = node->getSlot(i);
            if (slot.provider != nullptr) {
                nodeMap[slot.provider]->connect(nodeMap[node], slot.name);
            }
        }
    });
    return nodeMap[node.sharedFromThis()];
}

NAMESPACE_SPH_END
