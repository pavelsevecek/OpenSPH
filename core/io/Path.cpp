#include "io/Path.h"

NAMESPACE_SPH_BEGIN

Path::Path(const std::string& path)
    : path(path) {
    this->convert();
}

bool Path::empty() const {
    return path.empty();
}

bool Path::isHidden() const {
    Path name = this->fileName();
    return !name.empty() && name.path[0] == '.';
}

bool Path::isAbsolute() const {
    return !path.empty() && path[0] == SEPARATOR;
}

bool Path::isRelative() const {
    return !path.empty() && !this->isAbsolute();
}

bool Path::isRoot() const {
    return path.size() == 1 && path[0] == '/';
}

bool Path::hasExtension() const {
    return !this->extension().empty();
}

Path Path::parentPath() const {
    std::size_t n = path.rfind(SEPARATOR);
    if (n == std::string::npos) {
        return Path();
    }
    if (n == path.size() - 1) {
        // last character, find again without it
        return Path(path.substr(0, n)).parentPath();
    }
    return Path(path.substr(0, n + 1));
}

Path Path::fileName() const {
    std::size_t n = path.rfind(SEPARATOR);
    if (n == std::string::npos) {
        // path is just a filename
        return Path(path);
    } else if (n == path.size() - 1) {
        // directory, extract the name without the ending separator
        return Path(path.substr(0, path.size() - 1)).fileName();
    }
    return Path(path.substr(n + 1));
}

Path Path::extension() const {
    const Path name = this->fileName();
    if (name.path.size() <= 1) {
        return Path();
    }
    const std::size_t n = name.path.find_last_of('.');
    if (n == 0 || n == std::string::npos) {
        return Path();
    }
    return Path(name.path.substr(n + 1));
}

std::string Path::native() const {
    /// \todo change for windows
    return path;
}

Path& Path::replaceExtension(const std::string& newExtension) {
    const Path name = this->fileName();
    if (name.empty() || name.path == "." || name.path == "..") {
        return *this;
    }
    // skip first character, files like '.gitignore' are hidden, not just extension without filename
    const std::size_t n = name.path.find_last_of('.');
    if (n == 0 || n == std::string::npos) {
        // no extension, append the new one
        if (!newExtension.empty()) {
            path += "." + newExtension;
        }
        return *this;
    } else {
        const std::size_t indexInPath = path.size() - name.path.size() + n;
        if (!newExtension.empty()) {
            path.replace(indexInPath + 1, std::string::npos, newExtension);
        } else {
            path = path.substr(0, indexInPath);
        }
    }
    return *this;
}

Path& Path::removeExtension() {
    const Path name = this->fileName();
    if (name.empty() || name.path == "." || name.path == "..") {
        return *this;
    }
    const std::size_t n = name.path.find_last_of('.');
    if (n == 0 || n == std::string::npos) {
        // no extension, do nothing
        return *this;
    } else {
        path = path.substr(0, path.size() - name.path.size() + n);
    }
    return *this;
}

Path& Path::removeSpecialDirs() {
    std::size_t n;
    while ((n = this->findFolder("..")) != std::string::npos) {
        Path parent = Path(path.substr(0, std::max(0, int(n) - 1))).parentPath();
        Path tail(path.substr(std::min(n + 3, path.size())));
        *this = parent / tail;
    }
    while ((n = this->findFolder(".")) != std::string::npos) {
        Path parent = Path(path.substr(0, n));
        Path tail(path.substr(std::min(n + 2, path.size())));
        *this = parent / tail;
    }
    return *this;
}

Path& Path::makeAbsolute() {
    if (this->empty() || this->isAbsolute()) {
        return *this;
    }
    *this = Path::currentPath() / *this;
    return this->removeSpecialDirs();
}

Path& Path::makeRelative() {
    if (this->empty() || this->isRelative()) {
        return *this;
    }
    Path wd = Path::currentPath();
    SPH_ASSERT(wd.isAbsolute() && this->isAbsolute());
    std::size_t n = 0;
    // find shared part of the path
    while (true) {
        const std::size_t m = wd.path.find(SEPARATOR, n);
        if (m == std::string::npos || wd.path.substr(0, m) != path.substr(0, m)) {
            break;
        } else {
            n = m + 1;
        }
    }
    const Path sharedPath(path.substr(0, n));
    Path newPath;
    // add .. for every directory not in path
    while (wd.path.substr(0, n + 1) != sharedPath.path) {
        wd = wd.parentPath();
        newPath /= Path("..");
    }
    // add the remaining path of the original path
    newPath /= Path(path.substr(n));
    return *this = std::move(newPath);
}

Path Path::currentPath() {
    constexpr Size bufferCnt = 1024;
    char buffer[bufferCnt];
    if (getcwd(buffer, sizeof(buffer))) {
        std::string path(buffer);
        return Path(path + SEPARATOR);
    } else {
        return Path();
    }
}

Path Path::operator/(const Path& other) const {
    if (path.empty()) {
        return Path(other.path);
    } else if (other.path.empty()) {
        return Path(path);
    } else {
        return Path(path + "/" + other.path);
    }
}

Path& Path::operator/=(const Path& other) {
    *this = *this / other;
    return *this;
}

bool Path::operator==(const Path& other) const {
    return path == other.path;
}

bool Path::operator!=(const Path& other) const {
    return path != other.path;
}

bool Path::operator<(const Path& other) const {
    return path < other.path;
}

std::ostream& operator<<(std::ostream& stream, const Path& path) {
    stream << path.path;
    return stream;
}

void Path::convert() {
    for (char& c : path) {
        if (c == '\\' || c == '/') {
            c = SEPARATOR;
        }
    }
    std::string duplicates = { SEPARATOR, SEPARATOR };
    std::size_t n;
    while ((n = path.find(duplicates)) != std::string::npos) {
        path.replace(n, 2, 1, SEPARATOR);
    }
}

std::size_t Path::findFolder(const std::string& folder) {
    if (path == folder || path.substr(0, std::min(path.size(), folder.size() + 1)) == folder + SEPARATOR) {
        return 0;
    }
    const size_t n = path.find(SEPARATOR + folder + SEPARATOR);
    if (n != std::string::npos) {
        return n + 1;
    }
    if (path.substr(std::max(0, int(path.size()) - int(folder.size()) - 1)) == SEPARATOR + folder) {
        return path.size() - folder.size();
    }
    return std::string::npos;
}

Path operator"" _path(const char* nativePath, const std::size_t size) {
    SPH_ASSERT(size > 0);
    return Path(nativePath);
}

NAMESPACE_SPH_END
