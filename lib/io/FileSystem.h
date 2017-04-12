#pragma once

#include "objects/Object.h"
#include <string>

NAMESPACE_SPH_BEGIN

/// Craetes a directory with given path. Creates all parent directories as well.
bool createDirectory(const std::string& path);

/// Reads the whole file into the string.
std::string readFile(const std::string& path);

NAMESPACE_SPH_END
