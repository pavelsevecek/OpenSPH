#include "mpi/Mpi.h"
#include "mpi/Serializable.h"
#include "thread/CheckFunction.h"
#include <mpi.h>

NAMESPACE_SPH_BEGIN

/// The macro is defined if mpi.h, so we cannot check for it sooner
#ifdef MPI_VERSION
#define SPH_MPI
#endif


#ifdef SPH_MPI

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

bool Mpi::isMaster() const {
    return this->getProcessRank() == 0;
}

std::string Mpi::getProcessorName() const {
    char name[MPI_MAX_PROCESSOR_NAME];
    int length;
    MPI_Get_processor_name(name, &length);
    return name;
}

void Mpi::registerData(ClonePtr<Serializable>&& creator) {
    creator->registerHandle(creators.size());
    creators.push(std::move(creator));
}

void Mpi::send(const Serializable& data, const Size dest) {
    const Size handle = data.handle();
    Array<uint8_t> buffer;
    data.serialize(buffer);
    MPI_Send(&buffer[0], buffer.size(), MPI_UNSIGNED_CHAR, dest, int(handle), MPI_COMM_WORLD);
}

void Mpi::broadcast(const Serializable&) {
    NOT_IMPLEMENTED;
}

void Mpi::barrier() {
    MPI_Barrier(MPI_COMM_WORLD);
}

Mpi::BarrierLock::~BarrierLock() {
    Mpi& mpi = Mpi::getInstance();
    mpi.barrier();
}

AutoPtr<Serializable> Mpi::receive(const Size source) {
    // get the size of the buffer
    MPI_Status status;
    MPI_Probe(source, 0, MPI_COMM_WORLD, &status);
    const Size handle = status.MPI_TAG;
    int size;
    MPI_Get_count(&status, MPI_UNSIGNED_CHAR, &size);

    // receive the data
    Array<uint8_t> buffer(size);
    MPI_Recv(&buffer[0], size, MPI_UNSIGNED_CHAR, source, handle, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // deserialize the data
    ClonePtr<Serializable> serializable = creators[handle];
    serializable->deserialize(buffer);
    return serializable;
}

AutoPtr<Serializable> Mpi::receive(const RecvSource source) {
    switch (source) {
    case RecvSource::ANYONE:
        return this->receive(MPI_ANY_SOURCE);
    default:
        NOT_IMPLEMENTED;
    }
}

#endif

NAMESPACE_SPH_END
