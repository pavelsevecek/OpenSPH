#pragma once

#include "objects/wrappers/AutoPtr.h"

NAMESPACE_SPH_BEGIN

/// \brief Task to be executed by one of available threads
class ITask : public Polymorphic {
public:
    /// Executes the task.
    virtual void operator()() = 0;
};

/// \brief Task representing a simple lambda, functor or other callable object.
template <typename TFunctor>
class SimpleTask : public ITask {
private:
    TFunctor functor;

public:
    SimpleTask(TFunctor&& functor)
        : functor(std::move(functor)) {}

    virtual void operator()() override {
        functor();
    }
};

/// Creates a simple task, utilizing type deduction
template <typename TFunctor>
AutoPtr<ITask> makeTask(TFunctor&& functor) {
    return makeAuto<SimpleTask<TFunctor>>(std::move(functor));
}

/// \todo
class IScheduler : public Polymorphic {
public:
    /// \brief Submits a task into the thread pool.
    ///
    /// The task will be executed asynchronously once tasks submitted before it are completed.
    virtual void submit(AutoPtr<ITask>&& task) = 0;

    /// Blocks until all submitted tasks has been finished.
    virtual void waitForAll() = 0;
};

NAMESPACE_SPH_END
