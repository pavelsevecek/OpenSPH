#pragma once

#include "io/Path.h"
#include "objects/wrappers/Flags.h"
#include "objects/wrappers/Outcome.h"

NAMESPACE_SPH_BEGIN

/// Checks if a file exists (or more precisely, if a file is accessible).
bool pathExists(const Path& path);

enum class CreateDirectoryFlag {
    /// If the named directory already exists, function returns SUCCESS instead of error message
    ALLOW_EXISTING = 1 << 0
};

/// Creates a directory with given path. Creates all parent directories as well.
Outcome createDirectory(const Path& path,
    const Flags<CreateDirectoryFlag> flags = CreateDirectoryFlag::ALLOW_EXISTING);

/// Removes directory.
Outcome removeDirectory(const Path& path);

/// Reads the whole file into the string.
std::string readFile(const Path& path);

NAMESPACE_SPH_END
