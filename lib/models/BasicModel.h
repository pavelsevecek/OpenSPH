#pragma once

/// Definition of physical model.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "models/AbstractModel.h"
#include "objects/Object.h"
#include "objects/finders/Finder.h"
#include "sph/kernel/Kernel.h"
#include "storage/QuantityMap.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN


class BasicModel : public Abstract::Model {
private:
    std::unique_ptr<Abstract::Finder> finder;
    LutKernel kernel;



public:
    using View = BasicView;

    BasicModel(const Settings<GlobalSettingsIds>& settings);

    virtual void compute(Storage& storage) override { (void)storage; }
};

NAMESPACE_SPH_END
