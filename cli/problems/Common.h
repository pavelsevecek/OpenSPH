#pragma once

#include "Sph.h"
#include <fstream>
#include <iostream>

NAMESPACE_SPH_BEGIN

static const Path REFERENCE_DIR = Path("/home/pavel/projects/astro/sph/src/cli/problems/reference/");

inline Outcome areFilesEqual(const Path& path1, const Path& path2) {
    if (!FileSystem::pathExists(path1) || !FileSystem::pathExists(path2)) {
        return "One or both files do not exist";
    }
    if (FileSystem::fileSize(path1) != FileSystem::fileSize(path2)) {
        return "Files have different sizes";
    }
    std::ifstream ifs1(path1.native());
    std::ifstream ifs2(path2.native());
    StaticArray<char, 1024> buffer1, buffer2;

    while (ifs1 && ifs2) {
        ifs1.read(&buffer1[0], buffer1.size());
        ifs2.read(&buffer2[0], buffer2.size());

        if (buffer1 != buffer2) {
            return "Difference found at position " + std::to_string(ifs1.tellg());
        }
    }

    return SUCCESS;
}

class ProgressLog : public PeriodicTrigger {
public:
    ProgressLog(const Float period)
        : PeriodicTrigger(period, 0._f) {
        std::cout << std::endl;
    }

    virtual AutoPtr<ITrigger> action(Storage& UNUSED(storage), Statistics& stats) override {
        const Float progress = stats.get<Float>(StatisticsId::RELATIVE_PROGRESS);
        std::cout << int(progress * 100) << "%" << std::endl;
        return nullptr;
    }
};

NAMESPACE_SPH_END
