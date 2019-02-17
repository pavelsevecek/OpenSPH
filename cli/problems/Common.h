#pragma once

#include "Sph.h"
#include <fstream>
#include <iostream>

NAMESPACE_SPH_BEGIN

static const Path REFERENCE_DIR = Path("/home/pavel/projects/astro/sph/src/cli/problems/reference/");

/// \brief Checks whether the two files are identical (to the bit).
Outcome areFilesIdentical(const Path& path1, const Path& path2);

/// \brief Checks if two .ssf files are *almost* equal (may have eps-differences in quantities).
Outcome areFilesApproxEqual(const Path& path1, const Path& path2);

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
