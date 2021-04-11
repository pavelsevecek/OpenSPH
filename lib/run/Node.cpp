#include "run/Node.h"
#include "quantities/Quantity.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

JobNode::JobNode(AutoPtr<IJob>&& worker)
    : job(std::move(worker)) {}

std::string JobNode::className() const {
    return job->className();
}

std::string JobNode::instanceName() const {
    return job->instanceName();
}

VirtualSettings JobNode::getSettings() const {
    return job->getSettings();
}

JobType JobNode::provides() const {
    return job->provides();
}

void JobNode::connect(SharedPtr<JobNode> node, const std::string& slotName) {
    UnorderedMap<std::string, JobType> slots = node->job->getSlots();
    if (slots.contains(slotName)) {
        if (job->provides() != slots[slotName]) {
            throw InvalidSetup("Cannot connect node '" + job->instanceName() + "' to slot '" + slotName +
                               "' of node '" + node->instanceName() +
                               "', the slot expects different type of node.");
        }

        node->providers.insert(slotName, this->sharedFromThis());
        dependents.push(node);
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
    const UnorderedMap<std::string, JobType> slots = job->getSlots();
    const UnorderedMap<std::string, JobType> required = job->requires();
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

void JobNode::enumerate(Function<void(SharedPtr<JobNode>, Size)> func) {
    std::set<JobNode*> visited;
    this->enumerate(func, 0, visited);
}

void JobNode::enumerate(Function<void(SharedPtr<JobNode> job, Size depth)> func,
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

void JobNode::run(const RunSettings& global, IJobCallbacks& callbacks, std::set<JobNode*>& visited) {

    // first, run all dependencies
    for (auto& element : providers) {
        if (!job->requires().contains(element.key)) {
            // worker may change its requirements during (or before) the run, in this case it's not a real
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

    AutoPtr<IJob> worker = desc->create(name.empty() ? clonedName(node.instanceName()) : name);
    VirtualSettings target = worker->getSettings();
    VirtualSettings source = node.getSettings();
    CopyEntriesProc proc(target);
    source.enumerate(proc);

    return makeAuto<JobNode>(std::move(worker));
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
