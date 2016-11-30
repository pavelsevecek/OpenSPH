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

    static std::unique_ptr<Abstract::Finder> getFinder(const Settings<GlobalSettingsIds>& settings);

    static std::unique_ptr<Abstract::Distribution> getDistribution(const Settings<BodySettingsIds>& settings);

    static std::unique_ptr<Abstract::BoundaryConditions> getBoundaryConditions(
        const Settings<GlobalSettingsIds>& settings,
        std::unique_ptr<Abstract::Domain>&& domain);

    static std::unique_ptr<Abstract::Domain> getDomain(const Settings<GlobalSettingsIds>& settings);

    template <int d>
    static LutKernel<d> getKernel(const Settings<GlobalSettingsIds>& settings) {
        const KernelEnum id = settings.get<KernelEnum>(GlobalSettingsIds::SPH_KERNEL);
        switch (id) {
        case KernelEnum::CUBIC_SPLINE:
            return LutKernel<d>(CubicSpline<d>());
        case KernelEnum::FOURTH_ORDER_SPLINE:
            return LutKernel<d>(FourthOrderSpline<d>());
        case KernelEnum::CORE_TRIANGLE:
            ASSERT(d == 3);
            return LutKernel<3>(CoreTriangle());
        default:
            NOT_IMPLEMENTED;
        }
    }
};

NAMESPACE_SPH_END
