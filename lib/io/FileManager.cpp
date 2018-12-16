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
    auto iter = std::find(usedPaths.begin(), usedPaths.end(), expected);
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
            path = Path(path.native() + "_" + ss.str());
            path.replaceExtension(expected.extension().native());

            // starting from i==2, the path are always consecutive, so we don't really have to find the path;
            // the speed different is quite small, though
            iter = std::find(iter, usedPaths.end(), path);
            if (iter == usedPaths.end()) {
                usedPaths.insert(path);
                return path;
            }
        }
    }
    throw PathError();
}

char RandomPathManager::chars[] = "abcdefghijklmnopqrstuvwxyz0123456789";

Path RandomPathManager::getPath(const std::string extension) {
    while (true) {
        std::string name;
        for (Size i = 0; i < 8; ++i) {
            // -1 for terminating zero, -2 is the last indexable position
            const Size index = min(Size(rng() * (sizeof(chars) - 1)), Size(sizeof(chars) - 2));
            name += chars[index];
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
