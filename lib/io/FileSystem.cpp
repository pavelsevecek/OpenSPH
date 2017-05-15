#include "common/Assert.h"
#include "io/FileSystem.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

NAMESPACE_SPH_BEGIN

bool createDirectory(const std::string& path) {
    ASSERT(!path.empty());
    return mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0;
}

std::string readFile(const std::string& path) {
    std::ifstream t(path.c_str());
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

NAMESPACE_SPH_END
