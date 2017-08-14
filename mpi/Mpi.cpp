#include "Mpi.h"
#include "thread/CheckFunction.h"
#include <mpi.h>

NAMESPACE_SPH_BEGIN

Mpi* Mpi::instance = nullptr;

Mpi& Mpi::getInstance() {
    if (!instance) {
        instance = new Mpi();
    }
    return *instance;
}

Mpi::Mpi() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    // initialize the environment
    MPI_Init(nullptr, nullptr);
}

Mpi::~Mpi() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    // finalize the environment
    MPI_Finalize();
}

void Mpi::shutdown() {
    delete instance;
    instance = nullptr;
}

Size Mpi::getCommunicatorSize() const {
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    return size;
}

Size Mpi::getProcessRank() const {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    ASSERT(rank < int(this->getCommunicatorSize()));
    return rank;
}

bool Mpi::isMain() const {
    return this->getProcessRank() == 0;
}

std::string Mpi::getProcessorName() const {
    char name[MPI_MAX_PROCESSOR_NAME];
    int length;
    MPI_Get_processor_name(name, &length);
    return name;
}

NAMESPACE_SPH_END
