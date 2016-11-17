#pragma once

#include "objects/Object.h"
#include "storage/Storage.h"
#include "system/Settings.h"
#include <memory>

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Domain;
}

namespace Abstract {
    class Model : public Polymorphic {
    protected:
        std::shared_ptr<Storage> storage;

    public:
        Model(const std::shared_ptr<Storage>& storage)
            : storage(storage) {}

        /// Computes derivatives of all time-dependent quantities
        virtual void compute(Storage& storage) = 0;

        /// Creates particles in storage and initializes all quantities to default values.
        /// \param n Expected number of particles.
        virtual Storage createParticles(Abstract::Domain* domain,
                                        const Settings<BodySettingsIds>& settings) const = 0;
    };
}

NAMESPACE_SPH_END
