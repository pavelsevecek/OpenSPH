#include "Common.h"
#include "quantities/Iterate.h"

NAMESPACE_SPH_BEGIN

Outcome areFilesIdentical(const Path& path1, const Path& path2) {
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

Outcome areFilesApproxEqual(const Path& path1, const Path& path2) {
    BinaryInput input;
    Storage storage1, storage2;
    Statistics stats1, stats2;
    Outcome o1 = input.load(path1, storage1, stats1);
    Outcome o2 = input.load(path2, storage2, stats2);
    if (!o1 || !o2) {
        return o1 && o2;
    }
    if (storage1.getParticleCnt() != storage2.getParticleCnt()) {
        return "Different particle counts";
    }
    if (storage1.getMaterialCnt() != storage2.getMaterialCnt()) {
        return "Different material counts";
    }
    if (storage1.getQuantityCnt() != storage2.getQuantityCnt()) {
        return "Different quantity counts";
    }

    bool buffersEqual = true;
    iteratePair<VisitorEnum::ALL_BUFFERS>(storage1, storage2, [&buffersEqual](auto& b1, auto& b2) {
        if (b1.size() != b2.size()) {
            buffersEqual = false;
        } else {
            for (Size i = 0; i < b1.size(); ++i) {
                buffersEqual &= almostEqual(b1[i], b2[i], EPS);
            }
        }
    });

    /// \todo also materials
    if (!buffersEqual) {
        return "Different quantity values";
    } else {
        return SUCCESS;
    }
}

void measureRun(const Path& file, Function<void()> run) {
    Timer timer;
    run();
    const int64_t duration = timer.elapsed(TimerUnit::SECOND);
    FileLogger logger(file, FileLogger::Options::APPEND);
    logger.write("\"", __DATE__, "\"  ", duration);
}

NAMESPACE_SPH_END
