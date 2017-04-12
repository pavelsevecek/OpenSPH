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
    class Rheology;
    class Damage;
    class Distribution;
    class Domain;
    class Finder;
    class TimeStepping;
    class TimeStepCriterion;
    class BoundaryConditions;
    class Material;
    class Logger;
    class Output;
    class Rng;
}


class Storage;

/// Class providing construction of objects from enums. Contain only static member functions.
namespace Factory {
    std::unique_ptr<Abstract::Eos> getEos(const BodySettings& settings);

    std::unique_ptr<Abstract::Rheology> getRheology(const BodySettings& settings);

    std::unique_ptr<Abstract::Damage> getDamage(const BodySettings& settings);

    std::unique_ptr<Abstract::TimeStepping> getTimeStepping(const GlobalSettings& settings,
        const std::shared_ptr<Storage>& storage);

    std::unique_ptr<Abstract::TimeStepCriterion> getTimeStepCriterion(const GlobalSettings& settings);

    std::unique_ptr<Abstract::Finder> getFinder(const GlobalSettings& settings);

    std::unique_ptr<Abstract::Distribution> getDistribution(const BodySettings& settings);

    std::unique_ptr<Abstract::BoundaryConditions> getBoundaryConditions(const GlobalSettings& settings,
        std::unique_ptr<Abstract::Domain>&& domain);

    std::unique_ptr<Abstract::Domain> getDomain(const GlobalSettings& settings);

    std::unique_ptr<Abstract::Material> getMaterial(const BodySettings& settings);

    std::unique_ptr<Abstract::Logger> getLogger(const GlobalSettings& settings);

    std::unique_ptr<Abstract::Rng> getRng(const GlobalSettings& settings);

    template <int d>
    LutKernel<d> getKernel(const GlobalSettings& settings) {
        const KernelEnum id = settings.get<KernelEnum>(GlobalSettingsIds::SPH_KERNEL);
        switch (id) {
        case KernelEnum::CUBIC_SPLINE:
            return CubicSpline<d>();
        case KernelEnum::FOURTH_ORDER_SPLINE:
            return FourthOrderSpline<d>();
        case KernelEnum::GAUSSIAN:
            return Gaussian<d>();
        case KernelEnum::CORE_TRIANGLE:
            ASSERT(d == 3);
            return CoreTriangle();
        default:
            NOT_IMPLEMENTED;
        }
    }
}


NAMESPACE_SPH_END
