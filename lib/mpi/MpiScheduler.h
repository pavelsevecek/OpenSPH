#pragma once

/// \file MpiScheduler.h
/// \brief Implementation of IScheduler interface using MPI
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "mpi/Mpi.h"
#include "mpi/Serializable.h"
#include "thread/IScheduler.h"
#include <queue>

NAMESPACE_SPH_BEGIN

/// \brief Scheduler using MPI for parallelization.
///
/// Only serializable tasks can be submitted; it a non-serializable task is pushed, an exception is thrown.
class MpiScheduler : public IScheduler {
private:
    Mpi& mpi;

    /// Local queue of tasks
    std::queue<AutoPtr<ITask>> queue;


public:
    MpiScheduler()
        : mpi(Mpi::getInstance()) {}

    virtual void submit(AutoPtr<ITask>&& task) override {
        ISerializable& serializable = dynamic_cast<ISerializable&>(*task);
        Array<uint8_t> buffer;
        serializable.serialize(buffer);
        queue.push(std::move(task));
    }

    virtual void waitForAll() override {
        ASSERT(mpi.isMaster());
        mpi.barrier();
    }
};

NAMESPACE_SPH_END
