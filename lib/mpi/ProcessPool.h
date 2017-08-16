#pragma once

#include "mpi/Mpi.h"
#include "mpi/Serializable.h"

NAMESPACE_SPH_BEGIN

class ProcessPool {
public:
    void submit(const SerializableTask& task);


    void waitForAll() {
        Mpi& mpi = Mpi::getInstance();
        mpi.barrier();
    }
};

NAMESPACE_SPH_END
