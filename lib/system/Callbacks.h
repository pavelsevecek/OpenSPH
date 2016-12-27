#pragma once

/// Generic callbacks from the run, useful for GUI extensions.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Callbacks : public Polymorphic {
    public:
        virtual void onTimeStep(const std::shared_ptr<Storage>& storage) = 0;
    };
}

NAMESPACE_SPH_END
