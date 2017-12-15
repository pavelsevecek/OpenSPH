#pragma once

#include "objects/containers/Array.h"
#include "objects/wrappers/ClonePtr.h"
#include <map>

NAMESPACE_SPH_BEGIN

class ISerializable;

enum class RecvSource {
    ANYONE,
};

/// \brief Wrapper of mpi functionality
class Mpi {
private:
    /// Global instance of the object
    static Mpi* instance;

    std::map<Size, ClonePtr<ISerializable>> creators;

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
    /// The handle of the serializable must be unique. If multiple objects with the same handle are
    /// registered, an exception is thrown. To change a serializable, make sure to unrecord the previous one.
    /// Note that all serializables must be the same on all threads in the communicator, otherwise the
    /// behavior is undefined.
    void record(ClonePtr<ISerializable>&& creator);

    /// \brief Removes a serializable with given handle from the list.
    ///
    /// This allows a new serializable with the same handle to be registerd.
    /// \param handle The handle of the serializable to remove
    /// \return True if the serializable with given handle has been found and removed, false otherwise.
    bool unrecord(const Size handle);

    /// \brief Sends a serializable objects to given process.
    ///
    /// The function is blocking, it waits until the target process receives all data.
    /// \param data Serializable object sent over to the process.
    /// \param dest Rank of the target process.
    void send(const ISerializable& data, const Size dest);

    /// \brief Sends a serializable object to all processes.
    void broadcast(const ISerializable& data);

    /// \brief Blocks until all processes call the barrier.
    void barrier();

    /// \brief Receive a serializable object from given process.
    ///
    /// The function is blocking, it waits until all data are received.
    /// \param source Process sending the data.
    /// \return Object constructed from deserialized data
    ClonePtr<ISerializable> receive(const Size source);

    /// \brief Receive a serializable object from any process.
    ClonePtr<ISerializable> receive(const RecvSource source);

private:
    Mpi();
};

NAMESPACE_SPH_END
