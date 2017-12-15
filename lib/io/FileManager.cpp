#include "io/FileManager.h"
#include "io/FileSystem.h"
#include <iomanip>

NAMESPACE_SPH_BEGIN


class PathError : public std::exception {
public:
    virtual const char* what() const noexcept {
        return "Cannot generate more paths";
    }
};

Path UniquePathManager::getPath(const Path& expected) {
    if (std::find(usedPaths.begin(), usedPaths.end(), expected) == usedPaths.end()) {
        usedPaths.push(expected);
        return expected;
    } else {
        for (Size i = 1; i < 999; ++i) {
            Path path = expected;
            path.removeExtension();
            std::stringstream ss;
            ss << std::setw(3) << std::setfill('0') << i;
            path = Path(path.native() + "_" + ss.str());
            path.replaceExtension(expected.extension().native());

            if (std::find(usedPaths.begin(), usedPaths.end(), path) == usedPaths.end()) {
                usedPaths.push(path);
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
