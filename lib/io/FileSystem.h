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

/// Iterator allowing to enumerate all files in given directory
class DirectoryIterator {
private:
    /// \todo possibly hide through pimpl
    DIR* dir;
    dirent* entry = nullptr;

public:
    DirectoryIterator(DIR* dir);

    DirectoryIterator& operator++();

    Path operator*() const;

    bool operator!=(const DirectoryIterator& other) const;
};

class DirectoryAdapter : public Noncopyable {
private:
    DIR* dir;

public:
    DirectoryAdapter(const Path& directory);

    DirectoryAdapter(DirectoryAdapter&& other);

    ~DirectoryAdapter();

    DirectoryIterator begin() const;

    DirectoryIterator end() const;
};

DirectoryAdapter iterateDirectory(const Path& directory);


NAMESPACE_SPH_END
