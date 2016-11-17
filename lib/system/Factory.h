#pragma once

/// Creating program components based on values from settings.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "sph/kernel/Kernel.h"
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
    class BoundaryConditions;
}


class Storage;

/// Class providing construction of objects from enums. Contain only static member functions.
class Factory : public Noncopyable {
public:
    static std::unique_ptr<Abstract::Eos> getEos(const Settings<BodySettingsIds>& settings);

    static std::unique_ptr<Abstract::TimeStepping> getTimestepping(
        const Settings<GlobalSettingsIds>& settings,
        const std::shared_ptr<Storage>& storage);

    static std::unique_ptr<Abstract::Finder> getFinder(const FinderEnum id);

    static std::unique_ptr<Abstract::Distribution> getDistribution(const Settings<BodySettingsIds>& settings);

    static std::unique_ptr<Abstract::BoundaryConditions> getBoundaryConditions(
        const Settings<GlobalSettingsIds>& settings,
        const std::shared_ptr<Storage>& storage,
        std::unique_ptr<Abstract::Domain>&& domain);

    template <int d>
    static LutKernel<d> getKernel(const KernelEnum id) {
        switch (id) {
        case KernelEnum::CUBIC_SPLINE:
            return LutKernel<d>(CubicSpline<d>());
        default:
            ASSERT(false);
            throw std::exception();
        }
    }
};

/*class StaticFactory : public Noncopyable {
    template <typename TCreator>
    static void getSymmetrization(const Settings<GlobalSettingsIds>& settings, TCreator&& creator) {
        Optional<SmoothingLengthEnum> id = optionalCast<SmoothingLengthEnum>(
            settings.get<int>(GlobalSettingsIds::SMOOTHING_LENGTH_SYMMETRIZATION));
        switch (settings) {
        case SmoothingLengthEnum::SYMMETRIZE_KERNELS:
            creator.template create<SymW>();
            return;
        case SmoothingLengthEnum::SYMMETRIZE_LENGTHS:
            creator.template create<SymH>();
            return;
        }
    }
};*/


NAMESPACE_SPH_END
