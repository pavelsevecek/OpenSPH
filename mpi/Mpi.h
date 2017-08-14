#pragma once

#include "objects/containers/Array.h"
#include "objects/wrappers/AutoPtr.h"
#include <functional>

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

    using SerializableCreator = std::function<AutoPtr<Serializable>()>;

    Array<SerializableCreator> creators;


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
    bool isMain() const;

    /// \brief Returns the name of the processor running the process.
    ///
    /// Useful mainly for debugging purposes. The name is implementation defined and should not be relied on.
    std::string getProcessorName() const;

    template <typename TSerializable, typename... TArgs>
    void registerSerializable(TArgs... args) {
        creators.push([args...] { return makeAuto<TSerializable>(args...); });
    }

    void send(const Serializable& data, const Size dest);

    AutoPtr<Serializable> receive(const Size source);

    AutoPtr<Serializable> receive(const RecvSource source);

private:
    Mpi();
};

NAMESPACE_SPH_END
