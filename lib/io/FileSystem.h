#pragma once

#include "objects/wrappers/Flags.h"
#include "objects/wrappers/Outcome.h"
#include <string>

NAMESPACE_SPH_BEGIN

/// Checks if a file exists (or more precisely, if a file is accessible).
bool fileExists(const std::string& path);

enum class CreateDirectoryFlag {
    /// If the named directory already exists, function returns SUCCESS instead of error message
    ALLOW_EXISTING = 1 << 0
};

/// Creates a directory with given path. Creates all parent directories as well.
Outcome createDirectory(const std::string& path,
    const Flags<CreateDirectoryFlag> flags = CreateDirectoryFlag::ALLOW_EXISTING);

/// Removes directory.
Outcome removeDirectory(const std::string& path);

/// Reads the whole file into the string.
std::string readFile(const std::string& path);

NAMESPACE_SPH_END
