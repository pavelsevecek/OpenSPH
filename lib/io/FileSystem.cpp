#include "io/FileSystem.h"
#include "objects/containers/StaticArray.h"
#include <fstream>
#include <sstream>
#ifdef SPH_MSVC
#include <windows.h>
#else
#include <dirent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
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
    return Size(ifs.tellg());
}

bool FileSystem::isPathWritable(const Path& path) {
#ifdef SPH_MSVC
    MARK_USED(path);
    NOT_IMPLEMENTED;
#else
    return access(path.native().c_str(), W_OK) == 0;
#endif
}

Expected<Path> FileSystem::getHomeDirectory() {
    const char* homeDir = getenv("HOME");
    if (homeDir != nullptr) {
        return Path(homeDir);
    } else {
        return makeUnexpected<Path>("Cannot obtain home directory");
    }
}

Path FileSystem::getAbsolutePath(const Path& relativePath) {
#ifdef SPH_MSVC
    constexpr Size BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    GetFullPathNameA(relativePath.native().c_str(), BUFFER_SIZE, buffer, nullptr);
    return Path(buffer);
#else
    char realPath[PATH_MAX];
    realpath(relativePath.native().c_str(), realPath);
    return Path(realPath);
#endif
}

Expected<FileSystem::PathType> FileSystem::pathType(const Path& path) {
    if (path.empty()) {
        return makeUnexpected<PathType>("Path is empty");
    }

#ifdef SPH_MSVC
    const DWORD attributes = GetFileAttributesA(path.native().c_str());
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        return PathType::DIRECTORY;
    }
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        return PathType::SYMLINK;
    }
    return PathType::FILE;
#else
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
#endif
}


static Outcome createSingleDirectory(const Path& path, const Flags<FileSystem::CreateDirectoryFlag> flags) {
    ASSERT(!path.empty());
#ifdef SPH_MSVC
    MARK_USED(flags);
    NOT_IMPLEMENTED;
#else
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
#endif
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
#ifdef SPH_MSVC
    SetCurrentDirectoryA(path.native().c_str());
#else
    chdir(path.native().c_str());
#endif
}


#ifdef SPH_MSVC
class FileSystem::PlatformDirectoryData {
private:
    HANDLE handle;
    WIN32_FIND_DATAA data;

public:
    PlatformDirectoryData(const Path& directory) {
        const Path fileMask = directory / Path("*");
        handle = FindFirstFileA(fileMask.native().c_str(), &data);
        if (handle == INVALID_HANDLE_VALUE) {
            handle = nullptr;
        }
    }

    PlatformDirectoryData() {
        handle = nullptr;
    }

    ~PlatformDirectoryData() {
        if (handle) {
            FindClose(handle);
        }
    }

    void nextFile() {
        if (handle) {
            const bool success = FindNextFileA(handle, &data) != FALSE;
            if (!success) {
                FindClose(handle);
                handle = nullptr;
            }
        }
    }

    Path getFile() const {
        ASSERT(handle);
        return Path(data.cFileName);
    }

    bool operator==(const PlatformDirectoryData& other) const {
        // if both of the handles are nullptr, report as equal so that the loop ends
        // (end() is nullptr always, the other one only when it gets to the end).
        return !handle && !other.handle;
    }
};


#else

class FileSystem::PlatformDirectoryData {
private:
    DIR* dir;
    dirent* entry = nullptr;

public:
    PlatformDirectoryData(const Path& directory) {
        if (pathExists(directory)) {
            dir = opendir(directory.native().c_str());
        } else {
            dir = nullptr;
        }
    }

    PlatformDirectoryData() {
        dir = nullptr;
    }

    ~PlatformDirectoryData() {
        if (dir) {
            closedir(dir);
        }
    }

    void nextFile() {
        if (!dir) {
            return *this;
        }
        do {
            entry = readdir(dir);
        } while (entry && (this->getFile() == Path(".") || this->getFile() == Path("..")));
    }

    Path getFile() const {
        ASSERT(entry);
        return Path(entry->d_name);
    }

    bool operator==(const PlatformDirectoryData& other) const {
        return (!entry && !other.entry) || entry == other.entry;
    }
};

#endif

FileSystem::DirectoryIterator::DirectoryIterator(SharedPtr<PlatformDirectoryData> data)
    : data(data) {
#ifndef SPH_MSVC
    // find first file
    data->nextFile();
#endif
}

FileSystem::DirectoryIterator::~DirectoryIterator() = default;

FileSystem::DirectoryIterator& FileSystem::DirectoryIterator::operator++() {
    data->nextFile();
    return *this;
}

Path FileSystem::DirectoryIterator::operator*() const {
    return data->getFile();
}

bool FileSystem::DirectoryIterator::operator==(const DirectoryIterator& other) const {
    return *data == *other.data;
}

bool FileSystem::DirectoryIterator::operator!=(const DirectoryIterator& other) const {
    return !(*this == other);
}


FileSystem::DirectoryAdapter::DirectoryAdapter(const Path& directory) {
    ASSERT(pathType(directory).valueOr(PathType::OTHER) == PathType::DIRECTORY);
    data = makeAuto<PlatformDirectoryData>(directory);
}

FileSystem::DirectoryAdapter::DirectoryAdapter(DirectoryAdapter&& other) = default;

FileSystem::DirectoryAdapter::~DirectoryAdapter() = default;

FileSystem::DirectoryIterator FileSystem::DirectoryAdapter::begin() const {
    return DirectoryIterator(data);
}

FileSystem::DirectoryIterator FileSystem::DirectoryAdapter::end() const {
    return DirectoryIterator(makeShared<PlatformDirectoryData>());
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
#ifdef SPH_MSVC
    MARK_USED(path);
    NOT_IMPLEMENTED;
#else
    handle = open(path.native().c_str(), O_RDONLY);
    TODO("check that the file exists and was correctly opened");
#endif
}

void FileSystem::FileLock::lock() {
#ifdef SPH_MSVC
    NOT_IMPLEMENTED;
#else
    flock(handle, LOCK_EX | LOCK_NB);
#endif
}

void FileSystem::FileLock::unlock() {
#ifdef SPH_MSVC
    NOT_IMPLEMENTED;
#else
    flock(handle, LOCK_UN | LOCK_NB);
#endif
}

bool FileSystem::isFileLocked(const Path& UNUSED(path)) {
    NOT_IMPLEMENTED;
}


NAMESPACE_SPH_END
