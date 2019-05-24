#include "run/Node.h"
#include "quantities/Quantity.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

WorkerNode::WorkerNode(AutoPtr<IWorker>&& worker)
    : worker(std::move(worker)) {}

std::string WorkerNode::className() const {
    return worker->className();
}

std::string WorkerNode::instanceName() const {
    return worker->instanceName();
}

VirtualSettings WorkerNode::getSettings() const {
    return worker->getSettings();
}

WorkerType WorkerNode::provides() const {
    return worker->provides();
}

void WorkerNode::connect(SharedPtr<WorkerNode> node, const std::string& slotName) {
    UnorderedMap<std::string, WorkerType> slots = node->worker->getSlots();
    if (slots.contains(slotName)) {
        if (worker->provides() != slots[slotName]) {
            throw InvalidSetup("Cannot connect node '" + worker->instanceName() + "' to slot '" + slotName +
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


void WorkerNode::disconnect(SharedPtr<WorkerNode> dependent) {
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
    SharedPtr<WorkerNode> self = this->sharedFromThis();
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

void WorkerNode::disconnectAll() {
    while (!dependents.empty()) {
        this->disconnect(dependents.back().lock());
    }
}

Size WorkerNode::getSlotCnt() const {
    return worker->getSlots().size();
}

SlotData WorkerNode::getSlot(const Size index) const {
    const UnorderedMap<std::string, WorkerType> slots = worker->getSlots();
    const UnorderedMap<std::string, WorkerType> required = worker->requires();
    if (index >= slots.size()) {
        throw InvalidSetup("Cannot query slot #" + std::to_string(index) + ", node '" +
                           worker->instanceName() + "' has only " + std::to_string(slots.size()) + " slots");
    }

    const auto iter = slots.begin() + index;
    const bool used = required.contains(iter->key);

    SharedPtr<WorkerNode> provider;
    if (auto optProvider = providers.tryGet(iter->key)) {
        provider = optProvider.value();
    }
    return { iter->key, iter->value, used, provider };
}

Size WorkerNode::getDependentCnt() const {
    return dependents.size();
}

SharedPtr<WorkerNode> WorkerNode::getDependent(const Size index) const {
    return dependents[index].lock();
}

void WorkerNode::enumerate(Function<void(SharedPtr<WorkerNode>, Size)> func) {
    std::set<WorkerNode*> visited;
    this->enumerate(func, 0, visited);
}

void WorkerNode::enumerate(Function<void(SharedPtr<WorkerNode> worker, Size depth)> func,
    Size depth,
    std::set<WorkerNode*>& visited) {
    auto pair = visited.insert(this);
    if (!pair.second) {
        return;
    }
    func(this->sharedFromThis(), depth);
    for (auto& element : providers) {
        element.value->enumerate(func, depth + 1, visited);
    }
}

void WorkerNode::run(const RunSettings& global, IWorkerCallbacks& callbacks) {
    std::set<WorkerNode*> visited;
    this->run(global, callbacks, visited);
}

void WorkerNode::run(const RunSettings& global, IWorkerCallbacks& callbacks, std::set<WorkerNode*>& visited) {

    // first, run all dependencies
    for (auto& element : providers) {
        if (!worker->requires().contains(element.key)) {
            // worker may change its requirements during (or before) the run, in this case it's not a real
            // dependency
            continue;
        }
        SharedPtr<WorkerNode> provider = element.value;
        if (visited.find(&*provider) == visited.end()) {
            visited.insert(&*provider);
            provider->run(global, callbacks, visited);
        }

        WorkerContext result = provider->worker->getResult();
        if (provider->getDependentCnt() > 1) {
            // dependents modify the result in place, so we need to clone it
            result = result.clone();
        }
        worker->inputs.insert(element.key, result);
    }

    if (callbacks.shouldAbortRun()) {
        return;
    }

    callbacks.onStart(*worker);
    worker->evaluate(global, callbacks);

    WorkerContext result = worker->getResult();
    if (SharedPtr<ParticleData> data = result.tryGetValue<ParticleData>()) {
        callbacks.onEnd(data->storage, data->stats);
    } else {
        callbacks.onEnd(Storage(), Statistics());
    }
}

NAMESPACE_SPH_END
