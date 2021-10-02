#include "io/FileManager.h"
#include "io/FileSystem.h"
#include <algorithm>
#include <iomanip>

NAMESPACE_SPH_BEGIN

class PathError : public std::exception {
public:
    virtual const char* what() const noexcept {
        return "Cannot generate more paths";
    }
};

Path UniquePathManager::getPath(const Path& expected) {
    auto iter = usedPaths.find(expected);
    if (iter == usedPaths.end()) {
        usedPaths.insert(expected);
        return expected;
    } else {
        std::stringstream ss;
        // since std::set is sorted, we actually do not need to search the whole container for other paths
        for (Size i = 1; i < 999; ++i) {
            Path path = expected;
            path.removeExtension();
            ss.str("");
            ss << std::setw(3) << std::setfill('0') << i;
            path = Path(path.string() + L'_' + String::fromAscii(ss.str().c_str()));
            if (!expected.extension().empty()) {
                // add back previously removed extension
                // note that replaceExtension would remove any other extensions the file might have
                path = Path(path.string() + '.' + expected.extension().string());
            }

            iter = usedPaths.find(path);
            if (iter == usedPaths.end()) {
                usedPaths.insert(path);
                return path;
            }
        }
    }
    throw PathError();
}

UniqueNameManager::UniqueNameManager(ArrayView<const String> initial) {
    for (const String& name : initial) {
        names.insert(name);
    }
}

String UniqueNameManager::getName(const String& name) {
    String tested = name;

    for (Size postfix = 1; postfix < 999; ++postfix) {
        if (names.find(tested) == names.end()) {
            names.insert(tested);
            return tested;
        } else {
            /// \todo increase the number if there is already a (x) at the end
            tested = name + " (" + toString(postfix) + ")";
        }
    }
    return name; /// \todo what to return?
}

char RandomPathManager::chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";

Path RandomPathManager::getPath(const String& extension) {
    while (true) {
        String name;
        for (Size i = 0; i < 8; ++i) {
            // -1 for terminating zero, -2 is the last indexable position
            const Size index = min(Size(rng() * (sizeof(chars) - 1)), Size(sizeof(chars) - 2));
            name += wchar_t(chars[index]);
        }
        Path path(name);
        if (!extension.empty()) {
            path.replaceExtension(extension);
        }
        if (!FileSystem::pathExists(path)) {
            return path;
        }
    }
}

NAMESPACE_SPH_END
