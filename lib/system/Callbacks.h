#pragma once

/// Generic callbacks from the run, useful for GUI extensions.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Callbacks : public Polymorphic {
    public:
        /// Called every timestep.
        /// \param progress Value betweeen 0 and 1 indicating current progress of the run.
        virtual void onTimeStep(const float progress, const std::shared_ptr<Storage>& storage) = 0;

        /// Returns whether current run should be aborted or not. Can be called any time.
        virtual bool shouldAbortRun() const = 0;
    };
}

NAMESPACE_SPH_END
