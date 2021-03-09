#include "run/Job.h"
#include "objects/containers/FlatSet.h"
#include "post/MeshFile.h"
#include "quantities/Quantity.h"
#include "run/IRun.h"
#include "sph/initial/MeshDomain.h"
#include "thread/CheckFunction.h"

NAMESPACE_SPH_BEGIN

class IJobData : public Polymorphic {};

template <typename TValue>
class WorkerData : public IJobData {
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
JobContext::JobContext(SharedPtr<TValue> value) {
    data = makeShared<WorkerData<TValue>>(value);
}

template JobContext::JobContext(SharedPtr<ParticleData> value);
template JobContext::JobContext(SharedPtr<IDomain> value);
template JobContext::JobContext(SharedPtr<IMaterial> value);

template <typename TValue>
SharedPtr<TValue> JobContext::getValue() const {
    SharedPtr<TValue> value = this->tryGetValue<TValue>();
    if (value) {
        return value;
    } else {
        throw InvalidSetup("Expected different type when accessing worker context.");
    }
}

template SharedPtr<ParticleData> JobContext::getValue() const;
template SharedPtr<IDomain> JobContext::getValue() const;
template SharedPtr<IMaterial> JobContext::getValue() const;

template <typename TValue>
SharedPtr<TValue> JobContext::tryGetValue() const {
    RawPtr<WorkerData<TValue>> typedData = dynamicCast<WorkerData<TValue>>(data.get());
    if (typedData != nullptr) {
        return typedData->getValue();
    } else {
        return nullptr;
    }
}

template SharedPtr<ParticleData> JobContext::tryGetValue() const;
template SharedPtr<IDomain> JobContext::tryGetValue() const;
template SharedPtr<IMaterial> JobContext::tryGetValue() const;

JobContext JobContext::clone() const {
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
SharedPtr<T> IJob::getInput(const std::string& name) const {
    if (!inputs.contains(name)) {
        throw InvalidSetup(
            "Input '" + name + "' for worker '" + instName +
            "' was not found, either it was not connected or the node has not been successfully evaluated.");
    }

    JobContext context = inputs[name];
    return context.getValue<T>();
}

template SharedPtr<ParticleData> IJob::getInput<ParticleData>(const std::string& name) const;
template SharedPtr<IDomain> IJob::getInput<IDomain>(const std::string& name) const;
template SharedPtr<IMaterial> IJob::getInput<IMaterial>(const std::string& name) const;

static Array<AutoPtr<IJobDesc>> sRegisteredJobs;

ArrayView<const AutoPtr<IJobDesc>> enumerateRegisteredJobs() {
    return sRegisteredJobs;
}

RawPtr<IJobDesc> getJobDesc(const std::string& name) {
    for (auto& desc : sRegisteredJobs) {
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

JobRegistrar::JobRegistrar(std::string className,
    std::string shortName,
    std::string category,
    CreateJobFunc func,
    std::string tooltip) {

    class GenericDesc : public IJobDesc {
    private:
        std::string longName;
        std::string shortName;
        std::string cat;
        std::string desc;
        CreateJobFunc func;

    public:
        GenericDesc(std::string longName,
            std::string shortName,
            std::string cat,
            CreateJobFunc func,
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

        virtual AutoPtr<IJob> create(Optional<std::string> instanceName) const override {
            CHECK_FUNCTION(CheckFunction::NO_THROW);
            AutoPtr<IJob> worker = func(instanceName.valueOr("unnamed " + shortName));
            return worker;
        }
    };

    sRegisteredJobs.emplaceBack(makeAuto<GenericDesc>(className, shortName, category, func, tooltip));
}


JobRegistrar::JobRegistrar(std::string className,
    std::string category,
    CreateJobFunc func,
    std::string tooltip)
    : JobRegistrar(className, className, category, func, std::move(tooltip)) {}


IParticleJob::IParticleJob(const std::string& name)
    : IJob(name) {}

IParticleJob::~IParticleJob() = default;

IRunJob::IRunJob(const std::string& name)
    : IParticleJob(name) {}

IRunJob::~IRunJob() = default;

static SharedPtr<ParticleData> findStorageInput(const UnorderedMap<std::string, JobContext>& inputs,
    const std::string& workerName) {
    for (const auto& element : inputs) {
        SharedPtr<ParticleData> data = element.value.tryGetValue<ParticleData>();
        if (data != nullptr) {
            return data;
        }
    }
    throw InvalidSetup("No input particles found for worker '" + workerName + "'");
}

void IRunJob::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    SharedPtr<ParticleData> data = findStorageInput(inputs, instName);
    RunSettings overrides = global;
    overrides.addEntries(data->overrides);

    AutoPtr<IRun> run = this->getRun(overrides);
    data->stats = run->run(data->storage, callbacks);
    result = std::move(data);
}

NAMESPACE_SPH_END
