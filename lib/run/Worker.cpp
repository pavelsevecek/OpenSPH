#include "run/Worker.h"
#include "objects/containers/FlatSet.h"
#include "post/MeshFile.h"
#include "quantities/Quantity.h"
#include "run/IRun.h"
#include "sph/initial/MeshDomain.h"
#include "thread/CheckFunction.h"

NAMESPACE_SPH_BEGIN

class IWorkerData : public Polymorphic {};

template <typename TValue>
class WorkerData : public IWorkerData {
private:
    static_assert(std::is_same<TValue, ParticleData>::value || std::is_same<TValue, IDomain>::value ||
                      std::is_same<TValue, IMaterial>::value,
        "Invalid type");

    SharedPtr<TValue> value;

public:
    explicit WorkerData(SharedPtr<TValue> value)
        : value(value) {}

    SharedPtr<TValue> getValue() const {
        return value;
    }
};

template <typename TValue>
WorkerContext::WorkerContext(SharedPtr<TValue> value) {
    data = makeShared<WorkerData<TValue>>(value);
}

template WorkerContext::WorkerContext(SharedPtr<ParticleData> value);
template WorkerContext::WorkerContext(SharedPtr<IDomain> value);
template WorkerContext::WorkerContext(SharedPtr<IMaterial> value);

template <typename TValue>
SharedPtr<TValue> WorkerContext::getValue() const {
    SharedPtr<TValue> value = this->tryGetValue<TValue>();
    if (value) {
        return value;
    } else {
        throw InvalidSetup("Expected different type when accessing worker context.");
    }
}

template SharedPtr<ParticleData> WorkerContext::getValue() const;
template SharedPtr<IDomain> WorkerContext::getValue() const;
template SharedPtr<IMaterial> WorkerContext::getValue() const;

template <typename TValue>
SharedPtr<TValue> WorkerContext::tryGetValue() const {
    RawPtr<WorkerData<TValue>> typedData = dynamicCast<WorkerData<TValue>>(data.get());
    if (typedData != nullptr) {
        return typedData->getValue();
    } else {
        return nullptr;
    }
}

template SharedPtr<ParticleData> WorkerContext::tryGetValue() const;
template SharedPtr<IDomain> WorkerContext::tryGetValue() const;
template SharedPtr<IMaterial> WorkerContext::tryGetValue() const;

WorkerContext WorkerContext::clone() const {
    if (SharedPtr<ParticleData> particleData = this->tryGetValue<ParticleData>()) {
        SharedPtr<ParticleData> clonedData = makeShared<ParticleData>();
        clonedData->storage = particleData->storage.clone(VisitorEnum::ALL_BUFFERS);
        clonedData->overrides = particleData->overrides;
        clonedData->stats = particleData->stats;
        return clonedData;
    } else {
        // materials and domains are currently never modified by workers, so we can share them
        return *this;
    }
}

template <typename T>
SharedPtr<T> IWorker::getInput(const std::string& name) const {
    if (!inputs.contains(name)) {
        throw InvalidSetup(
            "Input '" + name + "' for worker '" + instName +
            "' was not found, either it was not connected or the node has not been successfully evaluated.");
    }

    WorkerContext context = inputs[name];
    return context.getValue<T>();
}

template SharedPtr<ParticleData> IWorker::getInput<ParticleData>(const std::string& name) const;
template SharedPtr<IDomain> IWorker::getInput<IDomain>(const std::string& name) const;
template SharedPtr<IMaterial> IWorker::getInput<IMaterial>(const std::string& name) const;

static Array<AutoPtr<IWorkerDesc>> sRegisteredWorkers;

ArrayView<const AutoPtr<IWorkerDesc>> enumerateRegisteredWorkers() {
    return sRegisteredWorkers;
}

RawPtr<IWorkerDesc> getWorkerDesc(const std::string& name) {
    for (auto& desc : sRegisteredWorkers) {
        if (desc->className() == name) {
            return desc.get();
        }
    }
    return nullptr;
}

VirtualSettings::Category& addGenericCategory(VirtualSettings& connector, std::string& instanceName) {
    VirtualSettings::Category& cat = connector.addCategory("Generic");
    cat.connect("Name", "name", instanceName);
    return cat;
}

WorkerRegistrar::WorkerRegistrar(std::string className,
    std::string shortName,
    std::string category,
    CreateWorkerFunc func,
    std::string tooltip) {

    class GenericDesc : public IWorkerDesc {
    private:
        std::string longName;
        std::string shortName;
        std::string cat;
        std::string desc;
        CreateWorkerFunc func;

    public:
        GenericDesc(std::string longName,
            std::string shortName,
            std::string cat,
            CreateWorkerFunc func,
            std::string desc)
            : longName(std::move(longName))
            , shortName(std::move(shortName))
            , cat(std::move(cat))
            , desc(std::move(desc))
            , func(func) {}

        virtual std::string className() const override {
            return longName;
        }

        virtual std::string category() const override {
            return cat;
        }

        virtual std::string tooltip() const override {
            return desc;
        }

        virtual AutoPtr<IWorker> create(Optional<std::string> instanceName) const override {
            CHECK_FUNCTION(CheckFunction::NO_THROW);
            AutoPtr<IWorker> worker = func(instanceName.valueOr("unnamed " + shortName));
            return worker;
        }
    };

    sRegisteredWorkers.emplaceBack(makeAuto<GenericDesc>(className, shortName, category, func, tooltip));
}


WorkerRegistrar::WorkerRegistrar(std::string className,
    std::string category,
    CreateWorkerFunc func,
    std::string tooltip)
    : WorkerRegistrar(className, className, category, func, std::move(tooltip)) {}


IParticleWorker::IParticleWorker(const std::string& name)
    : IWorker(name) {}

IParticleWorker::~IParticleWorker() = default;

IRunWorker::IRunWorker(const std::string& name)
    : IParticleWorker(name) {}

IRunWorker::~IRunWorker() = default;

static SharedPtr<ParticleData> findStorageInput(const UnorderedMap<std::string, WorkerContext>& inputs,
    const std::string& workerName) {
    for (const auto& element : inputs) {
        SharedPtr<ParticleData> data = element.value.tryGetValue<ParticleData>();
        if (data != nullptr) {
            return data;
        }
    }
    throw InvalidSetup("No input particles found for worker '" + workerName + "'");
}

void IRunWorker::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    SharedPtr<ParticleData> data = findStorageInput(inputs, instName);
    RunSettings overrides = global;
    overrides.addEntries(data->overrides);

    AutoPtr<IRun> run = this->getRun(overrides);
    run->run(data->storage, callbacks);
    result = std::move(data);
}

NAMESPACE_SPH_END
