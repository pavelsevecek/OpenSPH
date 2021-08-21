#include "io/Path.h"
#include "math/MathBasic.h"

#ifndef SPH_WIN
#include <unistd.h>
#else
#include <windows.h>
#endif

NAMESPACE_SPH_BEGIN

Path::Path(const String& path)
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
#ifndef SPH_WIN
    return !path.empty() && path[0] == SEPARATOR;
#else
    return path.size() >= 2 && path[0] >= 'A' && path[0] <= 'Z' && path[1] == ':';
#endif
}

bool Path::isRelative() const {
    return !path.empty() && !this->isAbsolute();
}

bool Path::isRoot() const {
#ifndef SPH_WIN
    return path.size() == 1 && path[0] == SEPARATOR;
#else
    // C: or C:/
    if (path.size() >= 2 && path.size() <= 3) {
        bool root = path[0] >= 'A' && path[0] <= 'Z' && path[1] == ':';
        if (path.size() == 3) {
            root &= (path[2] == SEPARATOR);
        }
        return root;
    } else {
        return false;
    }
#endif
}

bool Path::hasExtension() const {
    return !this->extension().empty();
}

Path Path::parentPath() const {
    const Size n = path.findLast(SEPARATOR);
    if (n == String::npos) {
        return Path();
    }
    if (n == path.size() - 1) {
        // last character, find again without it
        return Path(path.substr(0, n)).parentPath();
    }
    return Path(path.substr(0, n + 1));
}

Path Path::fileName() const {
    const Size n = path.findLast(SEPARATOR);
    if (n == String::npos) {
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
    const Size n = name.path.findLast('.');
    if (n == 0 || n == String::npos) {
        return Path();
    }
    return Path(name.path.substr(n + 1));
}

String Path::string() const {
    return path;
}

Path::NativePath Path::native() const {
#ifndef SPH_WIN
    return path.toUtf8();
#else
    return path.toUnicode();
#endif
}

Path& Path::replaceExtension(const String& newExtension) {
    const Path name = this->fileName();
    if (name.empty() || name.path == L"." || name.path == L"..") {
        return *this;
    }
    // skip first character, files like '.gitignore' are hidden, not just extension without filename
    const Size n = name.path.findLast('.');
    if (n == 0 || n == String::npos) {
        // no extension, append the new one
        if (!newExtension.empty()) {
            path += L'.' + newExtension;
        }
        return *this;
    } else {
        const Size indexInPath = path.size() - name.path.size() + n;
        if (!newExtension.empty()) {
            path.replace(indexInPath + 1, String::npos, newExtension);
        } else {
            path = path.substr(0, indexInPath);
        }
    }
    return *this;
}

Path& Path::removeExtension() {
    const Path name = this->fileName();
    if (name.empty() || name.path == L"." || name.path == L"..") {
        return *this;
    }
    const Size n = name.path.findLast(L'.');
    if (n == 0 || n == String::npos) {
        // no extension, do nothing
        return *this;
    } else {
        path = path.substr(0, path.size() - name.path.size() + n);
    }
    return *this;
}

Path& Path::removeSpecialDirs() {
    Size n;
    while ((n = this->findFolder(L"..")) != String::npos) {
        Path parent = Path(path.substr(0, max(0, int(n) - 1))).parentPath();
        Path tail(path.substr(min(n + 3, path.size())));
        *this = parent / tail;
    }
    while ((n = this->findFolder(L".")) != String::npos) {
        Path parent = Path(path.substr(0, n));
        Path tail(path.substr(min(n + 2, path.size())));
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
    Size n = 0;
    // find shared part of the path
    while (true) {
        const Size m = wd.path.find(SEPARATOR, n);
        if (m == String::npos || wd.path.substr(0, m) != path.substr(0, m)) {
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
        newPath /= Path(L"..");
    }
    // add the remaining path of the original path
    newPath /= Path(path.substr(n));
    return *this = std::move(newPath);
}

Path Path::currentPath() {
#ifndef SPH_WIN
    constexpr Size bufferCnt = 1024;
    char buffer[bufferCnt];
    if (getcwd(buffer, sizeof(buffer))) {
        String path = String::fromUtf8(buffer);
        return Path(path + SEPARATOR);
    } else {
        return Path();
    }
#else
    wchar_t path[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, path);
    return Path(String(path) + SEPARATOR);
#endif
}

Path Path::operator/(const Path& other) const {
    if (path.empty()) {
        return Path(other.path);
    } else if (other.path.empty()) {
        return Path(path);
    } else {
        return Path(path + L'/' + other.path);
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

std::wostream& operator<<(std::wostream& stream, const Path& path) {
    stream << path.path;
    return stream;
}

#ifndef SPH_WIN
static String DOUBLE_SEPARATOR = String(L"//");
#else
static String DOUBLE_SEPARATOR = String(L"\\\\");
#endif

void Path::convert() {
    for (wchar_t& c : path) {
        if (c == L'\\' || c == L'/') {
            c = SEPARATOR;
        }
    }
    while (path.find(DOUBLE_SEPARATOR) != String::npos) {
        path.replaceAll(DOUBLE_SEPARATOR, SEPARATOR);
    }
}

Size Path::findFolder(const String& folder) {
    if (path == folder || path.substr(0, min(path.size(), folder.size() + 1)) == folder + SEPARATOR) {
        return 0;
    }
    const Size n = path.find(SEPARATOR + folder + SEPARATOR);
    if (n != String::npos) {
        return n + 1;
    }
    if (path.substr(max(0, int(path.size()) - int(folder.size()) - 1)) == SEPARATOR + folder) {
        return path.size() - folder.size();
    }
    return String::npos;
}

Path operator"" _path(const wchar_t* nativePath, const std::size_t size) {
    SPH_ASSERT(size > 0);
    return Path(nativePath);
}

NAMESPACE_SPH_END
