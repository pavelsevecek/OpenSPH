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
    class Rheology;
    class Solver;
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

/// Class providing a convenient way to construct objects from settings.
namespace Factory {
    std::unique_ptr<Abstract::Eos> getEos(const BodySettings& settings);

    std::unique_ptr<Abstract::Rheology> getRheology(const BodySettings& settings);

    std::unique_ptr<Abstract::Damage> getDamage(const BodySettings& settings);

    std::unique_ptr<Abstract::TimeStepping> getTimeStepping(const RunSettings& settings,
        const std::shared_ptr<Storage>& storage);

    std::unique_ptr<Abstract::TimeStepCriterion> getTimeStepCriterion(const RunSettings& settings);

    std::unique_ptr<Abstract::Finder> getFinder(const RunSettings& settings);

    std::unique_ptr<Abstract::Distribution> getDistribution(const BodySettings& settings);

    std::unique_ptr<Abstract::Solver> getSolver(const RunSettings& settings);

    std::unique_ptr<Abstract::BoundaryConditions> getBoundaryConditions(const RunSettings& settings,
        std::unique_ptr<Abstract::Domain>&& domain);

    std::unique_ptr<Abstract::Domain> getDomain(const RunSettings& settings);

    std::unique_ptr<Abstract::Material> getMaterial(const BodySettings& settings);

    std::unique_ptr<Abstract::Logger> getLogger(const RunSettings& settings);

    std::unique_ptr<Abstract::Rng> getRng(const RunSettings& settings);
}


NAMESPACE_SPH_END
