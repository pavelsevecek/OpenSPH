#pragma once

#include "io/Path.h"
#include "objects/wrappers/Flags.h"
#include "objects/wrappers/Outcome.h"
#include <dirent.h>

NAMESPACE_SPH_BEGIN

/// Reads the whole file into the string. Returns an empty string if the file does not exist.
std::string readFile(const Path& path);

/// Checks if a file exists (or more precisely, if a file is accessible).
bool pathExists(const Path& path);

/// Returns the size of a file. The file must exist, checked by assert.
Size fileSize(const Path& path);

enum class CreateDirectoryFlag {
    /// If the named directory already exists, function returns SUCCESS instead of error message
    ALLOW_EXISTING = 1 << 0
};

/// Creates a directory with given path. Creates all parent directories as well.
Outcome createDirectory(const Path& path,
    const Flags<CreateDirectoryFlag> flags = CreateDirectoryFlag::ALLOW_EXISTING);


enum class RemovePathFlag {
    /// Removes also all subdirectories. If not used, removing non-empty directory will return an error.
    /// Option has no effect for files.
    RECURSIVE = 1 << 1
};

/// Removes a file or a directory at given path.
Outcome removePath(const Path& path, const Flags<RemovePathFlag> flags = EMPTY_FLAGS);

/// Iterator allowing to enumerate files in given directory
class DirectoryIterator {
private:
    /// \todo possibly hide through pimpl
    DIR* dir;
    dirent* entry = nullptr;

public:
    /// Creates an iterator for given directory entry. Can be nullptr, representing the one-past-last item in
    /// the directory (end iterator).
    DirectoryIterator(DIR* dir);

    /// Moves to the next file in the directory
    DirectoryIterator& operator++();

    /// Return the path of the file relative to the directory
    Path operator*() const;

    /// Checks for unequality. Returns false in case both iterators are nullptr.
    bool operator!=(const DirectoryIterator& other) const;
};

/// Object providing begin and end directory iterator for given directory path.
class DirectoryAdapter : public Noncopyable {
private:
    DIR* dir;

public:
    /// Creates the directory adapter for given path. In case the directory does not exist or isn't
    /// accessible, any calls of begin or end function return null iterator; range-based for loop for the
    /// adapter will be therefore perform zero iterations.
    DirectoryAdapter(const Path& directory);

    DirectoryAdapter(DirectoryAdapter&& other);

    ~DirectoryAdapter();

    /// Returns the directory iterator to the first entry in the directory. The files in directory are
    /// enumerated in unspecified order.
    DirectoryIterator begin() const;

    /// Returns the directory iterator to the one-past-last entry in the directory.
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

NAMESPACE_SPH_END
