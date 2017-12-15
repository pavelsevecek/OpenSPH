#include "io/FileSystem.h"
#include "objects/containers/StaticArray.h"
#include <fstream>
#include <sstream>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef SPH_USE_STD_EXPERIMENTAL
#include <experimental/filesystem>
#endif

NAMESPACE_SPH_BEGIN

std::string FileSystem::readFile(const Path& path) {
    std::ifstream t(path.native().c_str());
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

bool FileSystem::pathExists(const Path& path) {
    struct stat buffer;
    if (path.empty()) {
        return false;
    }
    return (stat(path.native().c_str(), &buffer) == 0);
}

Size FileSystem::fileSize(const Path& path) {
    std::ifstream ifs(path.native(), std::ifstream::ate | std::ifstream::binary);
    ASSERT(ifs);
    return ifs.tellg();
}

Expected<FileSystem::PathType> FileSystem::pathType(const Path& path) {
    if (path.empty()) {
        return makeUnexpected<PathType>("Path is empty");
    }
    struct stat buffer;
    if (stat(path.native().c_str(), &buffer) != 0) {
        /// \todo possibly get the actual error, like in other functions
        return makeUnexpected<PathType>("Cannot retrieve type of the path");
    }
    if (S_ISREG(buffer.st_mode)) {
        return PathType::FILE;
    }
    if (S_ISDIR(buffer.st_mode)) {
        return PathType::DIRECTORY;
    }
    if (S_ISLNK(buffer.st_mode)) {
        return PathType::SYMLINK;
    }
    return PathType::OTHER;
}


static Outcome createSingleDirectory(const Path& path, const Flags<FileSystem::CreateDirectoryFlag> flags) {
    ASSERT(!path.empty());
    const bool result = mkdir(path.native().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
    if (!result) {
        switch (errno) {
        case EACCES:
            return "Search permission is denied on a component of the path prefix, or write permission "
                   "is denied on the parent directory of the directory to be created.";
        case EEXIST:
            if (flags.has(FileSystem::CreateDirectoryFlag::ALLOW_EXISTING)) {
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

Outcome FileSystem::createDirectory(const Path& path, const Flags<CreateDirectoryFlag> flags) {
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

Outcome FileSystem::removePath(const Path& path, const Flags<RemovePathFlag> flags) {
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
    const Expected<PathType> type = pathType(path);
    if (!type) {
        return type.error();
    }
    if (type.value() == PathType::DIRECTORY && flags.has(RemovePathFlag::RECURSIVE)) {
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

Outcome FileSystem::copyFile(const Path& from, const Path& to) {
    ASSERT(pathType(from).valueOr(PathType::OTHER) == PathType::FILE);
    // there doesn't seem to be any system function for copying, so let's do it by hand
    std::ifstream ifs(from.native().c_str());
    if (!ifs) {
        return "Cannon open file " + from.native() + " for reading";
    }
    Outcome result = createDirectory(to.parentPath());
    if (!result) {
        return result;
    }
    std::ofstream ofs(to.native());
    if (!ofs) {
        return "Cannot open file " + to.native() + " for writing";
    }

    StaticArray<char, 1024> buffer;
    do {
        ifs.read(&buffer[0], buffer.size());
        ofs.write(&buffer[0], ifs.gcount());
        if (!ofs) {
            return "Failed from copy the file";
        }
    } while (ifs);
    return SUCCESS;
}


Outcome FileSystem::copyDirectory(const Path& from, const Path& to) {
    ASSERT(pathType(from).valueOr(PathType::OTHER) == PathType::DIRECTORY);
    Outcome result = createDirectory(to);
    if (!result) {
        return result;
    }
    for (Path path : iterateDirectory(from)) {
        const PathType type = pathType(from / path).valueOr(PathType::OTHER);
        switch (type) {
        case PathType::FILE:
            result = copyFile(from / path, to / path);
            break;
        case PathType::DIRECTORY:
            result = copyDirectory(from / path, to / path);
            break;
        default:
            // just ignore the rest
            result = SUCCESS;
        }
        if (!result) {
            return result;
        }
    }
    return SUCCESS;
}

void FileSystem::setWorkingDirectory(const Path& path) {
    ASSERT(pathType(path).valueOr(PathType::OTHER) == PathType::DIRECTORY);
    chdir(path.native().c_str());
}

FileSystem::DirectoryIterator::DirectoryIterator(DIR* dir)
    : dir(dir) {
    if (dir) {
        // find first file
        this->operator++();
    }
}

FileSystem::DirectoryIterator& FileSystem::DirectoryIterator::operator++() {
    if (!dir) {
        return *this;
    }
    do {
        entry = readdir(dir);
    } while (entry && (**this == Path(".") || **this == Path("..")));
    return *this;
}

Path FileSystem::DirectoryIterator::operator*() const {
    ASSERT(entry);
    return Path(entry->d_name);
}

bool FileSystem::DirectoryIterator::operator==(const DirectoryIterator& other) const {
    return (!entry && !other.entry) || entry == other.entry;
}

bool FileSystem::DirectoryIterator::operator!=(const DirectoryIterator& other) const {
    // returns false if both are nullptr to end the loop for nonexisting dirs
    return (entry || other.entry) && entry != other.entry;
}

FileSystem::DirectoryAdapter::DirectoryAdapter(const Path& directory) {
    ASSERT(pathType(directory).valueOr(PathType::OTHER) == PathType::DIRECTORY);
    if (!pathExists(directory)) {
        dir = nullptr;
        return;
    }
    dir = opendir(directory.native().c_str());
}

FileSystem::DirectoryAdapter::~DirectoryAdapter() {
    if (dir) {
        closedir(dir);
    }
}

FileSystem::DirectoryAdapter::DirectoryAdapter(DirectoryAdapter&& other)
    : dir(other.dir) {
    other.dir = nullptr;
}

FileSystem::DirectoryIterator FileSystem::DirectoryAdapter::begin() const {
    return DirectoryIterator(dir);
}

FileSystem::DirectoryIterator FileSystem::DirectoryAdapter::end() const {
    return DirectoryIterator(nullptr);
}

FileSystem::DirectoryAdapter FileSystem::iterateDirectory(const Path& directory) {
    return DirectoryAdapter(directory);
}

Array<Path> FileSystem::getFilesInDirectory(const Path& directory) {
    Array<Path> paths;
    for (Path path : iterateDirectory(directory)) {
        paths.push(path);
    }
    return paths;
}

FileSystem::FileLock::FileLock(const Path& path) {
    handle = open(path.native().c_str(), O_RDONLY);
    TODO("check that the file exists and was correctly opened");
}

void FileSystem::FileLock::lock() {
    flock(handle, LOCK_EX | LOCK_NB);
}

void FileSystem::FileLock::unlock() {
    flock(handle, LOCK_UN | LOCK_NB);
}

bool FileSystem::isFileLocked(const Path& UNUSED(path)) {
    NOT_IMPLEMENTED;
}


NAMESPACE_SPH_END
