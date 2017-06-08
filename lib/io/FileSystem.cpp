#include "io/FileSystem.h"
#include "common/Assert.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef SPH_USE_STD_EXPERIMENTAL
#include <experimental/filesystem>
#endif

NAMESPACE_SPH_BEGIN

bool pathExists(const Path& path) {
    struct stat buffer;
    return (stat(path.native().c_str(), &buffer) == 0);
}

namespace {
    Outcome createSingleDirectory(const Path& path, const Flags<CreateDirectoryFlag> flags) {
        ASSERT(!path.empty());
        const bool result = mkdir(path.native().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
        if (!result) {
            switch (errno) {
            case EACCES:
                return "Search permission is denied on a component of the path prefix, or write permission "
                       "is denied on the parent directory of the directory to be created.";
            case EEXIST:
                if (flags.has(CreateDirectoryFlag::ALLOW_EXISTING)) {
                    return SUCCESS;
                } else {
                    return "The named file exists.";
                }
            case ELOOP:
                return "A loop exists in symbolic links encountered during resolution of the path argument.";
            case EMLINK:
                return "The link count of the parent directory would exceed {LINK_MAX}.";
            case ENAMETOOLONG:
                return "The length of the path argument exceeds {PATH_MAX} or a pathname component is longer "
                       "than {NAME_MAX}.";
            case ENOENT:
                return "A component of the path prefix specified by path does not name an existing directory "
                       "or path is an empty string.";
            case ENOSPC:
                return "The file system does not contain enough space to hold the contents of the new "
                       "directory or to extend the parent directory of the new directory.";
            case ENOTDIR:
                return "A component of the path prefix is not a directory.";
            case EROFS:
                return "The parent directory resides on a read-only file system.";
            default:
                return "Unknown error";
            }
        }
        return SUCCESS;
    }
}

Outcome createDirectory(const Path& path, const Flags<CreateDirectoryFlag> flags) {
#ifdef SPH_USE_STD_EXPERIMENTAL
    return (Outcome)std::experimental::filesystem::create_directories(path);
#else
    const Path parentPath = path.parentPath();
    if (!pathExists(parentPath)) {
        const Outcome result = createDirectory(parentPath, flags);
        if (!result) {
            return result;
        }
    }
    return createSingleDirectory(path, flags);
#endif
}

Outcome removeDirectory(const Path& path) {
#ifdef SPH_USE_STD_EXPERIMENTAL
    try {
        std::experimental::filesystem::remove_all(path);
    } catch (std::experimental::filesystem::filesystem_error& e) {
        return e.what();
    }
    return SUCCESS;
#else
    ASSERT(!path.empty() && !path.isRoot());
    return (Outcome)(remove(path.native().c_str()) == 0);
#endif
}

std::string readFile(const Path& path) {
    std::ifstream t(path.native().c_str());
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

NAMESPACE_SPH_END
