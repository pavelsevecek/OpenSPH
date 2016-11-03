#pragma once

/// Generic callbacks from the run, useful for GUI extensions.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/ArrayView.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Callbacks : public Polymorphic {
    public:
        virtual void onTimeStep(ArrayView<const Vector> positions) = 0;
    };
}

NAMESPACE_SPH_END
