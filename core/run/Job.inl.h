#include "Job.h" // only for syntax highlighting

NAMESPACE_SPH_BEGIN

template <typename TValue>
class JobData : public IJobData {
private:
    SharedPtr<TValue> value;

public:
    explicit JobData(SharedPtr<TValue> value)
        : value(value) {}

    SharedPtr<TValue> getValue() const {
        return value;
    }
};

template <typename TValue>
JobContext::JobContext(SharedPtr<TValue> value) {
    data = makeShared<JobData<TValue>>(value);
}

template <typename TValue>
SharedPtr<TValue> JobContext::getValue() const {
    SharedPtr<TValue> value = this->tryGetValue<TValue>();
    if (value) {
        return value;
    } else {
        throw InvalidSetup("Expected different type when accessing job context.");
    }
}

template <typename TValue>
SharedPtr<TValue> JobContext::tryGetValue() const {
    RawPtr<JobData<TValue>> typedData = dynamicCast<JobData<TValue>>(data.get());
    if (typedData != nullptr) {
        return typedData->getValue();
    } else {
        return nullptr;
    }
}

template <typename T>
SharedPtr<T> IJob::getInput(const String& name) const {
    if (!inputs.contains(name)) {
        throw InvalidSetup(
            format("Input '{}' for job '{}' was not found, either it was not connected or the node has not "
                   "been successfully evaluated.",
                name,
                instName));
    }

    JobContext context = inputs[name];
    return context.getValue<T>();
}

NAMESPACE_SPH_END
