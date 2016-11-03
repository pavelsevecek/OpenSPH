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


class GenericStorage;
class LutKernel;


/// Class providing construction of objects from enums. Contain only static member functions.
class Factory : public Noncopyable {
public:
    static std::unique_ptr<Abstract::Eos> getEos(const EosEnum id, const Settings<BodySettingsIds>& settings);

    static std::unique_ptr<Abstract::TimeStepping> getTimestepping(
        const TimesteppingEnum id,
                const std::shared_ptr<GenericStorage>& storage,
        const Settings<GlobalSettingsIds>& settings);

    static std::unique_ptr<Abstract::Finder> getFinder(const FinderEnum id);

    static LutKernel getKernel(const KernelEnum id);
};

NAMESPACE_SPH_END
