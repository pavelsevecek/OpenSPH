#pragma once

#include "objects/Object.h"
#include <sys/stat.h>
#include <sys/types.h>

NAMESPACE_SPH_BEGIN

inline bool createDirectory(const std::string& path) {
    return mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0;
}

NAMESPACE_SPH_END
