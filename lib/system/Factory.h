#pragma once

/// Creating program components based on values from settings.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "system/Settings.h"
#include <memory>

NAMESPACE_SPH_BEGIN

// forward declarations
namespace Abstract {
    class Eos;
    class Distribution;
    class Domain;
    class Finder;
    class TimeStepping;
}


class Storage;
class LutKernel;


/// Class providing construction of objects from enums. Contain only static member functions.
class Factory : public Noncopyable {
public:
    static std::unique_ptr<Abstract::Eos> getEos(const Settings<BodySettingsIds>& settings);

    static std::unique_ptr<Abstract::TimeStepping> getTimestepping(
        const Settings<GlobalSettingsIds>& settings,
        const std::shared_ptr<Storage>& storage);

    static std::unique_ptr<Abstract::Finder> getFinder(const FinderEnum id);

    static std::unique_ptr<Abstract::Distribution> getDistribution(const Settings<BodySettingsIds>& settings);

    static LutKernel getKernel(const KernelEnum id);
};

NAMESPACE_SPH_END
