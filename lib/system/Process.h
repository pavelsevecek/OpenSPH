#pragma once

/// \file Process.h
/// \brief Creating and managing processes
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "io/Path.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

/// \brief Holds a handle to a created process.
///
/// The class allows to start, manage and kill a process. Note that the calling thread does not wait until the
/// created process exits, unless function wait (waitFor, waitUntil) is executed. In particular, the process
/// is not blocked in the destructor.
class Process {
private:
    pid_t childPid = -1;

public:
    /// Creates a null process handle.
    Process() = default;

    /// Creates a process by running given executable file.
    /// \param path Path to the executable. The file must exist.
    /// \param args Arguments passes to the executable. Can be an empty array.
    /// \throw Exception if the file does not exist or the process fails to start.
    Process(const Path& path, Array<std::string>&& args);

    /// Blocks the calling thread until the managed process exits. The function may block indefinitely.
    void wait();

    /// Blocks the calling thread until the managed process exits or for specified duration.
    /// \param duration Maximum wait duration in milliseconds.
    void waitFor(const uint64_t duration);

    /// Blocks the calling thread until the managed process exits or until given condition is met.
    /// \param condition Function returning true if the wait should be interrupted.
    /// \param checkEvery Period of condition checking in milliseconds
    void waitUntil(const std::function<bool()>& condition, const uint64_t checkEvery = 100);
};

NAMESPACE_SPH_END
