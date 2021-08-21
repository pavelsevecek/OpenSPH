#include "run/Job.h"
#include "objects/containers/FlatSet.h"
#include "post/MeshFile.h"
#include "quantities/Quantity.h"
#include "run/IRun.h"
#include "sph/initial/MeshDomain.h"
#include "thread/CheckFunction.h"

NAMESPACE_SPH_BEGIN

JobContext JobContext::clone() const {
    if (SharedPtr<ParticleData> particleData = this->tryGetValue<ParticleData>()) {
        SharedPtr<ParticleData> clonedData = makeShared<ParticleData>();
        clonedData->storage = particleData->storage.clone(VisitorEnum::ALL_BUFFERS);
        clonedData->overrides = particleData->overrides;
        clonedData->stats = particleData->stats;
        return clonedData;
    } else {
        // materials and domains are currently never modified by jobs, so we can share them
        return *this;
    }
}

void JobContext::release() {
    data.reset();
}

static Array<AutoPtr<IJobDesc>> sRegisteredJobs;

ArrayView<const AutoPtr<IJobDesc>> enumerateRegisteredJobs() {
    return sRegisteredJobs;
}

RawPtr<IJobDesc> getJobDesc(const String& name) {
    for (auto& desc : sRegisteredJobs) {
        if (desc->className() == name) {
            return desc.get();
        }
    }
    return nullptr;
}

VirtualSettings::Category& addGenericCategory(VirtualSettings& connector, String& instanceName) {
    VirtualSettings::Category& cat = connector.addCategory("Generic");
    cat.connect("Name", "name", instanceName);
    return cat;
}

JobRegistrar::JobRegistrar(String className,
    String shortName,
    String category,
    CreateJobFunc func,
    String tooltip) {

    class GenericDesc : public IJobDesc {
    private:
        String longName;
        String shortName;
        String cat;
        String desc;
        CreateJobFunc func;

    public:
        GenericDesc(String longName, String shortName, String cat, CreateJobFunc func, String desc)
            : longName(std::move(longName))
            , shortName(std::move(shortName))
            , cat(std::move(cat))
            , desc(std::move(desc))
            , func(func) {}

        virtual String className() const override {
            return longName;
        }

        virtual String category() const override {
            return cat;
        }

        virtual String tooltip() const override {
            return desc;
        }

        virtual AutoPtr<IJob> create(Optional<String> instanceName) const override {
            CHECK_FUNCTION(CheckFunction::NO_THROW);
            AutoPtr<IJob> job = func(instanceName.valueOr("unnamed " + shortName));
            return job;
        }
    };

    sRegisteredJobs.emplaceBack(makeAuto<GenericDesc>(className, shortName, category, func, tooltip));
}


JobRegistrar::JobRegistrar(String className, String category, CreateJobFunc func, String tooltip)
    : JobRegistrar(className, className, category, func, std::move(tooltip)) {}


IParticleJob::IParticleJob(const String& name)
    : IJob(name) {}

IParticleJob::~IParticleJob() = default;

IRunJob::IRunJob(const String& name)
    : IParticleJob(name) {}

IRunJob::~IRunJob() = default;

static SharedPtr<ParticleData> findStorageInput(const UnorderedMap<String, JobContext>& inputs,
    const String& jobName) {
    for (const auto& element : inputs) {
        SharedPtr<ParticleData> data = element.value().tryGetValue<ParticleData>();
        if (data != nullptr) {
            return data;
        }
    }
    throw InvalidSetup("No input particles found for job '" + jobName + "'");
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
