#pragma once

/// Definition of physical model.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include "sph/kernel/Kernel.h"
#include "objects/finders/Finder.h"
#include "storage/GenericStorage.h"
#include "system/Settings.h"
#include "storage/BasicView.h"

NAMESPACE_SPH_BEGIN


namespace Abstract {
    class Model : public Polymorphic {
    public:
        /// Computes derivatives of all time-dependent quantities
        virtual void compute(GenericStorage& storage) = 0;
    };
}

class BasicModel : public Abstract::Model {
private:
    std::unique_ptr<Abstract::Finder> finder;
    LutKernel kernel;

public:
    using View = BasicView;

    BasicModel(const Settings<GlobalSettingsIds>& settings);

    virtual void compute(GenericStorage& storage) override {
        (void)storage;
    }
};

NAMESPACE_SPH_END
