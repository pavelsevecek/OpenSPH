#pragma once

#include "io/Path.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/ClonePtr.h"
#include "objects/wrappers/Expected.h"
#include "objects/wrappers/Flags.h"
#include "objects/wrappers/Outcome.h"

NAMESPACE_SPH_BEGIN

namespace FileSystem {

/// \brief Reads the whole file into the string.
///
/// Returns an empty string if the file does not exist or cannot be opened for reading.
/// \param path File to read
std::string readFile(const Path& path);

/// \brief Checks if a file or directory exists (or more precisely, if a file or directory is accessible).
bool pathExists(const Path& path);

/// \brief Returns the size of a file.
///
/// The file must exist and be accessible, checked by assert.
std::size_t fileSize(const Path& path);

/// \brief Checks whether the given file is writable.
bool isPathWritable(const Path& path);

/// \brief Returns the home directory of the current user.
Expected<Path> getHomeDirectory();

/// \brief Returns the directory where user data can be stored.
Expected<Path> getUserDataDirectory();

/// \brief Returns the absolute path to the file, or error if the path cannot be resolved.
///
/// Function also resolves all symlinks in the path.
Expected<Path> getAbsolutePath(const Path& relativePath);

enum class PathType {
    FILE,      ///< Regular file
    DIRECTORY, ///< Directory
    SYMLINK,   ///< Symbolic link
    OTHER,     ///< Pipe, socket, ...
};

/// \brief Returns the type of the given path, or error message if the function fails.
Expected<PathType> pathType(const Path& path);


enum class CreateDirectoryFlag {
    /// If the named directory already exists, function returns SUCCESS instead of error message
    ALLOW_EXISTING = 1 << 0
};

/// \brief Creates a directory with given path. Creates all parent directories as well.
///
/// If empty path is passed, the function returns SUCCESS without actually doing anything, it is therefore
/// possible to use constructs like
/// \code createDirectory(file.parentPath()); \endcode
/// even with relative paths.
Outcome createDirectory(const Path& path,
    const Flags<CreateDirectoryFlag> flags = CreateDirectoryFlag::ALLOW_EXISTING);

enum class RemovePathFlag {
    /// Removes also all subdirectories. If not used, removing non-empty directory will return an error.
    /// Option has no effect for files.
    RECURSIVE = 1 << 1
};

/// Removes a file or a directory at given path.
/// \param path Path to a file or directory to remove.
/// \param flags Optional parameters of the function, see RemovePathFlag enum.
/// \return SUCCESS if the path has been removed, or error message.
Outcome removePath(const Path& path, const Flags<RemovePathFlag> flags = EMPTY_FLAGS);

/// \brief Copies a file on given path to a different path.
///
/// The original file is not changed. If there is already a file on target path, the file is overriden.
/// All parent directories of target path are created, if necessary.
/// \param from Source file to be copied.
/// \param to Target path where the copy is saved.
/// \return SUCCESS if the file has been copied, or error message.
Outcome copyFile(const Path& from, const Path& to);

/// \brief Copies a directory (and all files and subdirectories it contains) to a different path.
///
/// The original directory is not changed. If there is already a directory on target path, it is
/// overriden. All parent directories of target path are created, if necessary.
Outcome copyDirectory(const Path& from, const Path& to);

/// \brief Changes the current working directory.
///
/// \return True on success.
bool setWorkingDirectory(const Path& path);

/// Helper RAII class, changing the working directory to given path when constructor and reverting it to
/// the original path in destructor.
class ScopedWorkingDirectory {
private:
    Path originalDir;

public:
    ScopedWorkingDirectory(const Path& path) {
        originalDir = Path::currentPath();
        setWorkingDirectory(path);
    }

    ~ScopedWorkingDirectory() {
        setWorkingDirectory(originalDir);
    }
};

/// Iterator allowing to enumerate files and subdirectories in given directory.
class DirectoryIterator {
    friend class DirectoryAdapter;

private:
    struct DirData;
    ClonePtr<DirData> data;

public:
    ~DirectoryIterator();

    /// Moves to the next file in the directory
    DirectoryIterator& operator++();

    /// Return the path of the file. The path is returned relative to the parent directory.
    Path operator*() const;

    /// Checks for equality. Returns true in case both iterators are nullptr.
    bool operator==(const DirectoryIterator& other) const;

    /// Checks for inequality. Returns false in case both iterators are nullptr.
    bool operator!=(const DirectoryIterator& other) const;

private:
    DirectoryIterator(ClonePtr<DirData> data);
};

/// \brief Object providing begin and end directory iterator for given directory path.
///
/// It allows to enumerate all files and subdirectories in given directory. The enumeration is not
/// recursive; if needed, another DirectoryAdapter has to be created for all subdirectories.
/// The enumeration skips directories '.' and '..'.
class DirectoryAdapter : public Noncopyable {
private:
    ClonePtr<DirectoryIterator::DirData> data;

public:
    /// \brief Creates the directory adapter for given path.
    ///
    /// In case the directory does not exist or isn't accessible, any calls of begin or end function
    /// return null iterator; range-based for loop for the adapter will be therefore perform zero
    /// iterations.
    DirectoryAdapter(const Path& directory);

    DirectoryAdapter(DirectoryAdapter&& other);

    ~DirectoryAdapter();

    /// \brief Returns the directory iterator to the first entry in the directory.
    ///
    /// The files in directory are enumerated in unspecified order.
    DirectoryIterator begin() const;

    /// \brief Returns the directory iterator to the one-past-last entry in the directory.
    DirectoryIterator end() const;
};

/// Syntactic suggar, function simply returning the DirectoryAdapter for given path.
///
/// Usage:
/// \code
/// for (Path path : iterateDirectory(parentDir) {
///     foo(parentDir / path);
/// }
/// \endcode
DirectoryAdapter iterateDirectory(const Path& directory);

/// \brief Alternatitve to \ref iterateDirectory, returning all files in directory in an array.
///
/// Returns relative paths with respect to the given parent directory.
Array<Path> getFilesInDirectory(const Path& directory);

} // namespace FileSystem

NAMESPACE_SPH_END
