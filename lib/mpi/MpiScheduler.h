#pragma once

#include "mpi/Mpi.h"
#include "mpi/Serializable.h"
#include "thread/IScheduler.h"

NAMESPACE_SPH_BEGIN

/// Only serializable tasks can be submitted
class MpiScheduler : public IScheduler {
private:
    Mpi& mpi;

    /// Number of tasks assigned to each thread.
    Array<Size> taskCnt;

public:
    MpiScheduler()
        : mpi(Mpi::getInstance()) {
        taskCnt.resize(mpi.getCommunicatorSize());
        taskCnt.fill(0);
    }

    virtual void submit(AutoPtr<ITask>&& task) override {
        ASSERT(mpi.isMaster());
        ISerializable& serializable = dynamic_cast<ISerializable&>(*task);
        // find the thread with the least tasks
    }

    virtual void waitForAll() override {
        ASSERT(mpi.isMaster());
        mpi.barrier();
    }
};

NAMESPACE_SPH_END
