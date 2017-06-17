#pragma once

/// \file Path.h
/// \brief Object representing a path on a filesystem, similar to std::filesystem::path in c++17
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/Assert.h"
#include <unistd.h>

NAMESPACE_SPH_BEGIN

/// \brief Object representing a path on a filesystem
///
/// Can represent both directory and file paths. Object does not check existence or accesibility of the path
/// in any way.
class Path {
private:
    std::string path;

    static constexpr char SEPARATOR = '/';

public:
    /// Constructs an empty path
    Path() = default;

    /// Constructs a path from string
    explicit Path(const std::string& path);


    /// \section Path queries

    /// Checks if the path is empty
    bool empty() const;

    /// Checks if the file is hidden, i.e. starts with a dot (makes only sense on Unit based systems).
    bool isHidden() const;

    /// Checks if the path is absolute.
    /// \todo generalize, currently only checks for path starting with '/'
    bool isAbsolute() const;

    /// Checks if the path is relative. Empty path is not considered relative.
    bool isRelative() const;

    /// Checks if the object holds root path.
    bool isRoot() const;

    /// Checks if the file has an extension.
    bool hasExtension() const;


    /// \section Parts of the path

    /// Returns the parent directory. If the path is empty or root, return empty path.
    Path parentPath() const;

    /// Returns the filename of the path. This is an empty path for directories.
    /// The file name includes the extension, if present.
    Path fileName() const;

    /// Returns the extension of the filename. This is an empty path for directories or for files without
    /// extensions. If the file has more than one extension, returns all of them.
    /// Example:
    ///   /usr/lib -> ""
    ///   file.txt -> "txt"
    ///   archive.tar.gz -> tar.gz
    ///   .gitignore -> ""
    Path extension() const;


    /// \section Format observers

    /// Returns the native version of the path
    std::string native() const;


    /// \section Path modifiers

    /// Changes the extension of the file. If the file has no extension, adds the given extension. For files
    /// with more than one extensions (.tar.gz, etc.), removes all extensions and adds the new one.
    /// \return Reference to itself, allowing to chain function calls
    Path& replaceExtension(const std::string& newExtension);

    /// Removes the extension from the path. If the path has no extension, the function does nothing.
    Path& removeExtension();

    /// Removes . and .. directories from the path
    Path& removeSpecialDirs();

    /// Turns the path into an absolute path. If the path already is absolute, function does nothing.
    Path& makeAbsolute();

    /// Turns the path into a relative path. If the path already is relative, function does nothing.
    Path& makeRelative();


    /// \section Static functions

    /// Returns the current working directory, or empty path if the function failed.
    static Path currentPath();


    /// \section Operators

    /// Appends two paths together.
    Path operator/(const Path& other) const;

    /// Appends another path to this one.
    Path& operator/=(const Path& other);

    /// Checks if two objects represent the same path. Note that this does not check for the actual file, so
    /// relative and absolute path to the same file are considered different, it ignores symlinks etc.
    bool operator==(const Path& other) const;

    /// Prints the path into the stream
    friend std::ostream& operator<<(std::ostream& stream, const Path& path);

private:
    /// Converts all separators into the selected one.
    void convert();

    /// Finds a given folder in a path.
    std::size_t findFolder(const std::string& folder);
};


NAMESPACE_SPH_END