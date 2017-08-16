#pragma once

#include "objects/containers/Array.h"
#include "objects/wrappers/ClonePtr.h"

NAMESPACE_SPH_BEGIN

class Serializable;

enum class RecvSource {
    ANYONE,
};

/// \brief Wrapper of mpi functionality
class Mpi {
private:
    /// Global instance of the object
    static Mpi* instance;

    Array<ClonePtr<Serializable>> creators;


public:
    ~Mpi();

    /// Returns the global instance of the Mpi class
    static Mpi& getInstance();

    /// Closes down the Mpi environment.
    static void shutdown();

    /// Returns the size of the communicator.
    Size getCommunicatorSize() const;

    /// \brief Returns the rank of the process in the communicator.
    ///
    /// The rank is within [0, communicator_size-1].
    Size getProcessRank() const;

    /// Returns true if the calling process is the main one (with rank 0).
    bool isMaster() const;

    /// \brief Returns the name of the processor running the process.
    ///
    /// Useful mainly for debugging purposes. The name is implementation defined and should not be relied on.
    std::string getProcessorName() const;

    /// \brief Registers a serializable that can be sent and received by MPI processes.
    ///
    /// All processes have to register the same serializables in the same order. The order of the creator is
    /// used as handle as the object!
    void registerData(ClonePtr<Serializable>&& creator);

    /// \brief Sends a serializable objects to given process.
    ///
    /// The function is blocking, it waits until the target process receives all data.
    /// \param data Serializable object sent over to the process.
    /// \param dest Rank of the target process.
    void send(const Serializable& data, const Size dest);

    /// \brief Sends a serializable object to all processes.
    void broadcast(const Serializable& data);

    /// \brief Blocks until all processes call the barrier.
    void barrier();

    /// \brief Helper object that calls Mpi::barrier() from its destructor.
    ///
    /// Useful for exception safe MPI code.
    struct BarrierLock {
        ~BarrierLock();
    };


    /// \brief Receive a serializable object from given process.
    ///
    /// The function is blocking, it waits until all data are received.
    /// \param source Process sending the data.
    /// \return Object constructed from deserialized data
    AutoPtr<Serializable> receive(const Size source);

    /// \brief Receive a serializable object from any process.
    AutoPtr<Serializable> receive(const RecvSource source);

private:
    Mpi();
};

NAMESPACE_SPH_END
