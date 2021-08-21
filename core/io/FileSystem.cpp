#include "io/FileSystem.h"
#include "objects/containers/StaticArray.h"
#include "objects/utility/Streams.h"
#include <sstream>

#ifdef SPH_WIN
#include <userenv.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

NAMESPACE_SPH_BEGIN

#ifdef SPH_WIN

/// Returns the error message corresponding to GetLastError().
static String getLastErrorMessage() {
    DWORD error = GetLastError();
    if (!error) {
        return {};
    }

    wchar_t message[256] = { 0 };
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD length =
        FormatMessageW(flags, NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), message, 256, nullptr);
    if (length) {
        return String(message);
    } else {
        return {};
    }
}

#endif

String FileSystem::readFile(const Path& path) {
    FileTextInputStream stream(path);
    String text;
    if (stream.readAll(text)) {
        return text;
    } else {
        return {};
    }
}

bool FileSystem::pathExists(const Path& path) {
#ifndef SPH_WIN
    struct stat buffer;
    if (path.empty()) {
        return false;
    }
    return (stat(path.native(), &buffer) == 0);
#else
    DWORD attributes = GetFileAttributesW(path.native());
    return attributes != INVALID_FILE_ATTRIBUTES;
#endif
}

std::size_t FileSystem::fileSize(const Path& path) {
    std::ifstream ifs(path.native(), std::ios::ate | std::ios::binary);
    SPH_ASSERT(ifs);
    return ifs.tellg();
}

bool FileSystem::isDirectoryWritable(const Path& path) {
    SPH_ASSERT(pathType(path).valueOr(PathType::OTHER) == PathType::DIRECTORY);
#ifndef SPH_WIN
    return access(path.native(), W_OK) == 0;
#else
    wchar_t file[MAX_PATH];
    GetTempFileNameW(path.native(), L"sph", 1, file);
    HANDLE handle = CreateFileW(file,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
        nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        return true;
    } else {
        return false;
    }
#endif
}

Expected<Path> FileSystem::getHomeDirectory() {
#ifdef SPH_WIN
    wchar_t buffer[MAX_PATH] = { 0 };
    DWORD length = sizeof(buffer);
    HANDLE token = 0;
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    if (GetUserProfileDirectoryW(token, buffer, &length)) {
        CloseHandle(token);
        return Path(String(buffer) + L'\\');
    } else {
        return makeUnexpected<Path>(getLastErrorMessage());
    }
#else
    const char* homeDir = getenv("HOME");
    if (homeDir != nullptr) {
        String path = String::fromUtf8(homeDir) + L'/';
        return Path(path);
    } else {
        return makeUnexpected<Path>("Cannot obtain home directory");
    }
#endif
}

Expected<Path> FileSystem::getUserDataDirectory() {
    Expected<Path> homeDir = getHomeDirectory();
    if (homeDir) {
#ifdef SPH_WIN
        return homeDir.value();
#else
        return homeDir.value() / Path(".config");
#endif
    } else {
        return homeDir;
    }
}

Expected<Path> FileSystem::getAbsolutePath(const Path& relativePath) {
#ifndef SPH_WIN
    char realPath[PATH_MAX];
    if (realpath(relativePath.native(), realPath)) {
        return Path(String::fromUtf8(realPath));
    } else {
        switch (errno) {
        case EACCES:
            return makeUnexpected<Path>(
                "Read or search permission was denied for a component of the path prefix.");
        case EINVAL:
            return makeUnexpected<Path>("Path is NULL.");
        case EIO:
            return makeUnexpected<Path>("An I/O error occurred while reading from the filesystem.");
        case ELOOP:
            return makeUnexpected<Path>(
                "Too many symbolic links were encountered in translating the pathname.");
        case ENAMETOOLONG:
            return makeUnexpected<Path>(
                "A component of a pathname exceeded NAME_MAX characters, or an entire pathname exceeded "
                "PATH_MAX characters.");
        case ENOENT:
            return makeUnexpected<Path>("The named file does not exist");
        case ENOMEM:
            return makeUnexpected<Path>("Out of memory");
        case ENOTDIR:
            return makeUnexpected<Path>("A component of the path prefix is not a directory.");
        default:
            return makeUnexpected<Path>("Unknown error");
        }
    }
#else
    wchar_t buffer[256] = { 0 };

    DWORD retval = GetFullPathNameW(relativePath.native(), 256, buffer, nullptr);
    if (retval) {
        return Path(buffer);
    } else {
        return makeUnexpected<Path>(getLastErrorMessage());
    }
#endif
}

Expected<FileSystem::PathType> FileSystem::pathType(const Path& path) {
    if (path.empty()) {
        return makeUnexpected<PathType>("Path is empty");
    }

#ifndef SPH_WIN
    struct stat buffer;
    if (stat(path.native(), &buffer) != 0) {
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
#else
    DWORD attributes = GetFileAttributesW(path.native());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return makeUnexpected<PathType>(getLastErrorMessage());
    }
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        return PathType::DIRECTORY;
    }
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        return PathType::SYMLINK;
    }
    return PathType::FILE;
#endif
}

static Outcome createSingleDirectory(const Path& path, const Flags<FileSystem::CreateDirectoryFlag> flags) {
    SPH_ASSERT(!path.empty());
#ifndef SPH_WIN
    const bool result = mkdir(path.native(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
    if (!result) {
        switch (errno) {
        case EACCES:
            return makeFailed(
                "Search permission is denied on a component of the path prefix, or write permission is "
                "denied on the parent directory of the directory to be created.");
        case EEXIST:
            if (flags.has(FileSystem::CreateDirectoryFlag::ALLOW_EXISTING)) {
                return SUCCESS;
            } else {
                return makeFailed("The named file exists.");
            }
        case ELOOP:
            return makeFailed(
                "A loop exists in symbolic links encountered during resolution of the path argument.");
        case EMLINK:
            return makeFailed("The link count of the parent directory would exceed {LINK_MAX}.");
        case ENAMETOOLONG:
            return makeFailed(
                "The length of the path argument exceeds {PATH_MAX} or a pathname component is longer "
                "than {NAME_MAX}.");
        case ENOENT:
            return makeFailed(
                "A component of the path prefix specified by path does not name an existing directory "
                "or path is an empty string.");
        case ENOSPC:
            return makeFailed(
                "The file system does not contain enough space to hold the contents of the new "
                "directory or to extend the parent directory of the new directory.");
        case ENOTDIR:
            return makeFailed("A component of the path prefix is not a directory.");
        case EROFS:
            return makeFailed("The parent directory resides on a read-only file system.");
        default:
            return makeFailed("Unknown error");
        }
    }
    return SUCCESS;
#else
    if (CreateDirectoryW(path.native(), nullptr)) {
        return SUCCESS;
    } else {
        if (GetLastError() == ERROR_ALREADY_EXISTS &&
            flags.has(FileSystem::CreateDirectoryFlag::ALLOW_EXISTING)) {
            return SUCCESS;
        }
        return makeFailed(getLastErrorMessage());
    }
#endif
}

Outcome FileSystem::createDirectory(const Path& path, const Flags<CreateDirectoryFlag> flags) {
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
}

Outcome FileSystem::removePath(const Path& path, const Flags<RemovePathFlag> flags) {
    if (path.empty()) {
        return makeFailed("Attempting to remove an empty path");
    }
    if (path.isRoot()) {
        return makeFailed("Attemping to remove root. Pls, don't ...");
    }
    if (!pathExists(path)) {
        return makeFailed("Attemping to remove nonexisting path");
    }
    const Expected<PathType> type = pathType(path);
    if (!type) {
        return makeFailed(type.error());
    }
    if (type.value() == PathType::DIRECTORY && flags.has(RemovePathFlag::RECURSIVE)) {
        for (Path child : iterateDirectory(path)) {
            Outcome result = removePath(path / child, flags);
            if (!result) {
                return result;
            }
        }
    }

#ifndef SPH_WIN
    const bool result = (remove(path.native()) == 0);
    if (!result) {
        switch (errno) {
        case EACCES:
            return makeFailed(
                "Write access to the directory containing pathname was not allowed, or one of the "
                "directories in the path prefix of pathname did not allow search permission.");
        case EBUSY:
            return makeFailed(
                "Path " + path.string() +
                "is currently in use by the system or some process that prevents its removal.On "
                "Linux this means pathname is currently used as a mount point or is the root directory of "
                "the calling process.");
        case EFAULT:
            return makeFailed("Path " + path.string() + " points outside your accessible address space.");
        case EINVAL:
            return makeFailed("Path " + path.string() + " has . as last component.");
        case ELOOP:
            return makeFailed("Too many symbolic links were encountered in resolving path " + path.string());
        case ENAMETOOLONG:
            return makeFailed("Path " + path.string() + " was too long.");
        case ENOENT:
            return makeFailed("A directory component in path " + path.string() +
                              " does not exist or is a dangling symbolic link.");
        case ENOMEM:
            return makeFailed("Insufficient kernel memory was available.");
        case ENOTDIR:
            return makeFailed(
                "Path " + path.string() +
                " or a component used as a directory in pathname, is not, in fact, a directory.");
        case ENOTEMPTY:
            return makeFailed(
                "Path " + path.string() +
                " contains entries other than . and ..; or, pathname has .. as its final component.");
        case EPERM:
            return makeFailed(
                "The directory containing path " + path.string() +
                " has the sticky bit(S_ISVTX) set and the process's "
                "effective user ID is neither the user ID of the file to be deleted nor that of the "
                "directory containing it, and the process is not privileged (Linux: does not have the "
                "CAP_FOWNER capability).");
        case EROFS:
            return makeFailed("Path " + path.string() + " refers to a directory on a read-only file system.");
        default:
            return makeFailed("Unknown error for path " + path.string());
        }
    }
    return SUCCESS;
#else
    BOOL result;
    if (type.value() == PathType::DIRECTORY) {
        result = RemoveDirectoryW(path.native());
    } else if (type.value() == PathType::FILE) {
        result = DeleteFileW(path.native());
    } else {
        NOT_IMPLEMENTED; // removing symlinks?
    }

    if (result) {
        return SUCCESS;
    } else {
        return makeFailed(getLastErrorMessage());
    }
#endif
}

Outcome FileSystem::copyFile(const Path& from, const Path& to) {
    SPH_ASSERT(pathType(from).valueOr(PathType::OTHER) == PathType::FILE);
    // there doesn't seem to be any system function for copying, so let's do it by hand
    std::ifstream ifs(from.native(), std::ios::in | std::ios::binary);
    if (!ifs) {
        return makeFailed("Cannon open file " + from.string() + " for reading");
    }
    Outcome result = createDirectory(to.parentPath());
    if (!result) {
        return result;
    }
    std::ofstream ofs(to.native(), std::ios::out | std::ios::binary);
    if (!ofs) {
        return makeFailed("Cannot open file " + to.string() + " for writing");
    }

    StaticArray<char, 1024> buffer;
    do {
        ifs.read(&buffer[0], buffer.size());
        ofs.write(&buffer[0], ifs.gcount());
        if (!ofs) {
            return makeFailed("Failed from copy the file");
        }
    } while (ifs);
    return SUCCESS;
}

Outcome FileSystem::copyDirectory(const Path& from, const Path& to) {
    SPH_ASSERT(pathType(from).valueOr(PathType::OTHER) == PathType::DIRECTORY);
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

bool FileSystem::setWorkingDirectory(const Path& path) {
    SPH_ASSERT(pathType(path).valueOr(PathType::OTHER) == PathType::DIRECTORY);
#ifndef SPH_WIN
    return chdir(path.native()) == 0;
#else
    return SetCurrentDirectoryW(path.native());
#endif
}

Expected<Path> FileSystem::getDirectoryOfExecutable() {
#ifndef SPH_WIN
    char result[4096];
    ssize_t count = readlink("/proc/self/exe", result, sizeof(result));
    if (count != -1) {
        Path path(String::fromUtf8(result));
        return path.parentPath();
    } else {
        return makeUnexpected<Path>("Unknown error");
    }
#else
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) != 0) {
        return Path(path).parentPath();
    } else {
        return makeUnexpected<Path>(getLastErrorMessage());
    }
#endif
}

struct FileSystem::DirectoryIterator::DirData {
#ifndef SPH_WIN
    DIR* dir = nullptr;
    dirent* entry = nullptr;
#else
    HANDLE dir = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW entry;
    DWORD error = 1;
#endif
};

static bool isSpecial(const Path& path) {
    return path == Path(".") || path == Path("..");
}

FileSystem::DirectoryIterator::DirectoryIterator(DirData&& dirData)
    : data(makeAuto<DirData>(std::move(dirData))) {
    SPH_ASSERT(data);
#ifndef SPH_WIN
    if (data->dir != nullptr) {
        // find first file
        this->operator++();
    }
#else
    // skip special paths
    while (data->dir != INVALID_HANDLE_VALUE && !data->error && isSpecial(**this)) {
        this->operator++();
    }
#endif
}

FileSystem::DirectoryIterator::DirectoryIterator(const DirectoryIterator& other) {
    *this = other;
}

FileSystem::DirectoryIterator& FileSystem::DirectoryIterator::operator=(const DirectoryIterator& other) {
    data = makeAuto<DirData>(*other.data);
    return *this;
}

FileSystem::DirectoryIterator::~DirectoryIterator() = default;

FileSystem::DirectoryIterator& FileSystem::DirectoryIterator::operator++() {
#ifndef SPH_WIN
    if (data->dir == nullptr) {
        return *this;
    }
    do {
        data->entry = readdir(data->dir);
    } while (data->entry && isSpecial(**this));
#else
    if (data->dir == INVALID_HANDLE_VALUE) {
        return *this;
    }
    bool result = true;
    do {
        result = FindNextFileW(data->dir, &data->entry);
    } while (result && isSpecial(**this));

    if (!result) {
        data->error = GetLastError();
    }
#endif
    return *this;
}

Path FileSystem::DirectoryIterator::operator*() const {
#ifndef SPH_WIN
    SPH_ASSERT(data && data->entry);
    return Path(String::fromUtf8(data->entry->d_name));
#else
    SPH_ASSERT(data && !data->error);
    return Path(data->entry.cFileName);
#endif
}

bool FileSystem::DirectoryIterator::operator==(const DirectoryIterator& other) const {
#ifndef SPH_WIN
    return (!data->entry && !other.data->entry) || data->entry == other.data->entry;
#else
    return (data->error && other.data->error) || data->dir == other.data->dir;
#endif
}

bool FileSystem::DirectoryIterator::operator!=(const DirectoryIterator& other) const {
    // returns false if both are nullptr to end the loop for nonexisting dirs
    return !(*this == other);
}

FileSystem::DirectoryAdapter::DirectoryAdapter(const Path& directory) {
    SPH_ASSERT(pathType(directory).valueOr(PathType::OTHER) == PathType::DIRECTORY);
    data = makeAuto<DirectoryIterator::DirData>();
#ifndef SPH_WIN
    if (!pathExists(directory)) {
        data->dir = nullptr;
        return;
    }
    data->dir = opendir(directory.native());
#else
    if (!pathExists(directory)) {
        data->dir = INVALID_HANDLE_VALUE;
        return;
    }
    String searchExpression = (directory / Path("*")).string();
    data->dir = FindFirstFileW(searchExpression.toUnicode(), &data->entry);
    if (data->dir != INVALID_HANDLE_VALUE) {
        data->error = 0;
    }
#endif
}

FileSystem::DirectoryAdapter::~DirectoryAdapter() {
#ifndef SPH_WIN
    if (data->dir) {
        closedir(data->dir);
    }
#else
    if (data->dir != INVALID_HANDLE_VALUE) {
        FindClose(data->dir);
    }
#endif
}

FileSystem::DirectoryAdapter::DirectoryAdapter(DirectoryAdapter&& other) {
    data = std::move(other.data);
    other.data = makeAuto<DirectoryIterator::DirData>();
}

FileSystem::DirectoryIterator FileSystem::DirectoryAdapter::begin() const {
    return DirectoryIterator(DirectoryIterator::DirData(*data));
}

FileSystem::DirectoryIterator FileSystem::DirectoryAdapter::end() const {
    return DirectoryIterator({});
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

NAMESPACE_SPH_END
