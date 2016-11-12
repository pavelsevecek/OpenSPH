#pragma once

#include "objects/Object.h"
#include "storage/Storage.h"

NAMESPACE_SPH_BEGIN


namespace Abstract {
    class Model : public Polymorphic {
    public:
        /// Computes derivatives of all time-dependent quantities
        virtual void compute(Storage& storage) = 0;

        ///virtual Storage createStorage(Settings) = 0;
    };
}

NAMESPACE_SPH_END
