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

std::string readFile(const Path& path) {
    std::ifstream t(path.native().c_str());
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

bool pathExists(const Path& path) {
    struct stat buffer;
    if (path.empty()) {
        return false;
    }
    return (stat(path.native().c_str(), &buffer) == 0);
}

Size fileSize(const Path& path) {
    std::ifstream ifs(path.native(), std::ifstream::ate | std::ifstream::binary);
    ASSERT(ifs);
    return ifs.tellg();
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
    if (path.empty()) {
        return SUCCESS;
    }
    const Path parentPath = path.parentPath();
    if (!parentPath.empty() && !pathExists(parentPath)) {
        const Outcome result = createDirectory(parentPath, flags);
        if (!result) {
            return result;
        }
    }
    return createSingleDirectory(path, flags);
#endif
}

Outcome removePath(const Path& path, const Flags<RemovePathFlag> flags) {
#ifdef SPH_USE_STD_EXPERIMENTAL
    try {
        std::experimental::filesystem::remove_all(path);
    } catch (std::experimental::filesystem::filesystem_error& e) {
        return e.what();
    }
    return SUCCESS;
#else
    if (path.empty()) {
        return "Attempting to remove an empty path";
    }
    if (path.isRoot()) {
        return "Attemping to remove root. Pls, don't ...";
    }
    if (!pathExists(path)) {
        return "Attemping to remove nonexisting path";
    }
    if (flags.has(RemovePathFlag::RECURSIVE)) {
        for (Path child : iterateDirectory(path)) {
            Outcome result = removePath(path / child, flags);
            if (!result) {
                return result;
            }
        }
    }

    const bool result = (remove(path.native().c_str()) == 0);
    if (!result) {
        switch (errno) {
        case EACCES:
            return "Write access to the directory containing pathname was not allowed, or one of the "
                   "directories in the path prefix of pathname did not allow search permission.";
        case EBUSY:
            return "Path " + path.native() +
                   "is currently in use by the system or some process that prevents its removal.On "
                   "Linux this means pathname is currently used as a mount point or is the root directory of "
                   "the calling process.";
        case EFAULT:
            return "Path " + path.native() + " points outside your accessible address space.";
        case EINVAL:
            return "Path " + path.native() + " has . as last component.";
        case ELOOP:
            return "Too many symbolic links were encountered in resolving path " + path.native();
        case ENAMETOOLONG:
            return "Path " + path.native() + " was too long.";
        case ENOENT:
            return "A directory component in path " + path.native() +
                   " does not exist or is a dangling symbolic link.";
        case ENOMEM:
            return "Insufficient kernel memory was available.";
        case ENOTDIR:
            return "Path " + path.native() +
                   " or a component used as a directory in pathname, is not, in fact, a directory.";
        case ENOTEMPTY:
            return "Path " + path.native() +
                   " contains entries other than . and ..; or, pathname has .. as its final component.";
        case EPERM:
            return "The directory containing path " + path.native() +
                   " has the sticky bit(S_ISVTX) set and the process's "
                   "effective user ID is neither the user ID of the file to be deleted nor that of the "
                   "directory containing it, and the process is not privileged (Linux: does not have the "
                   "CAP_FOWNER capability).";
        /*case EPERM:
            "The file system containing pathname does not support the removal of directories.";*/
        case EROFS:
            return "Path " + path.native() + " refers to a directory on a read-only file system.";
        default:
            return "Unknown error for path " + path.native();
        }
    }
    return SUCCESS;
#endif
}

void setWorkingDirectory(const Path& path) {
    chdir(path.native().c_str());
}

DirectoryIterator::DirectoryIterator(DIR* dir)
    : dir(dir) {
    if (dir) {
        // find first file
        this->operator++();
    }
}

DirectoryIterator& DirectoryIterator::operator++() {
    if (!dir) {
        return *this;
    }
    do {
        entry = readdir(dir);
    } while (entry && (**this == Path(".") || **this == Path("..")));
    return *this;
}

Path DirectoryIterator::operator*() const {
    ASSERT(entry);
    return Path(entry->d_name);
}

bool DirectoryIterator::operator!=(const DirectoryIterator& other) const {
    // returns false if both are nullptr to end the loop for nonexisting dirs
    return (entry || other.entry) && entry != other.entry;
}

DirectoryAdapter::DirectoryAdapter(const Path& directory) {
    if (!pathExists(directory)) {
        dir = nullptr;
        return;
    }
    dir = opendir(directory.native().c_str());
}

DirectoryAdapter::~DirectoryAdapter() {
    if (dir) {
        closedir(dir);
    }
}

DirectoryAdapter::DirectoryAdapter(DirectoryAdapter&& other)
    : dir(other.dir) {
    other.dir = nullptr;
}

DirectoryIterator DirectoryAdapter::begin() const {
    return DirectoryIterator(dir);
}

DirectoryIterator DirectoryAdapter::end() const {
    return DirectoryIterator(nullptr);
}

DirectoryAdapter iterateDirectory(const Path& directory) {
    return DirectoryAdapter(directory);
}

NAMESPACE_SPH_END
