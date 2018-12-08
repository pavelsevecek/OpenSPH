#pragma once

#include "Sph.h"
#include <iostream>

NAMESPACE_SPH_BEGIN

class ProgressLog : public PeriodicTrigger {
public:
    ProgressLog()
        : PeriodicTrigger(2.e-4, 0._f) {
        std::cout << std::endl;
    }

    virtual AutoPtr<ITrigger> action(Storage& UNUSED(storage), Statistics& stats) override {
        const Float progress = stats.get<Float>(StatisticsId::RELATIVE_PROGRESS);
        std::cout << int(progress * 100) << "%" << std::endl;
        return nullptr;
    }
};

NAMESPACE_SPH_END
