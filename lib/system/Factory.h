#pragma once

/// \file Factory.h
/// \brief Creating code components based on values from settings.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/wrappers/SharedPtr.h"
#include "system/Settings.h"

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
    class EquationTerm;
}


class Storage;

/// Class providing a convenient way to construct objects from settings.
namespace Factory {
    /// \todo move SPH specific stuff elsewhere
    AutoPtr<Abstract::Eos> getEos(const BodySettings& settings);

    AutoPtr<Abstract::Rheology> getRheology(const BodySettings& settings);

    AutoPtr<Abstract::Damage> getDamage(const BodySettings& settings);

    AutoPtr<Abstract::EquationTerm> getArtificialViscosity(const RunSettings& settings);

    AutoPtr<Abstract::TimeStepping> getTimeStepping(const RunSettings& settings,
        const SharedPtr<Storage>& storage);

    AutoPtr<Abstract::TimeStepCriterion> getTimeStepCriterion(const RunSettings& settings);

    AutoPtr<Abstract::Finder> getFinder(const RunSettings& settings);

    AutoPtr<Abstract::Distribution> getDistribution(const BodySettings& settings);

    AutoPtr<Abstract::Solver> getSolver(const RunSettings& settings);

    AutoPtr<Abstract::BoundaryConditions> getBoundaryConditions(const RunSettings& settings,
        AutoPtr<Abstract::Domain>&& domain);

    AutoPtr<Abstract::Domain> getDomain(const RunSettings& settings);

    AutoPtr<Abstract::Material> getMaterial(const BodySettings& settings);

    AutoPtr<Abstract::Logger> getLogger(const RunSettings& settings);

    AutoPtr<Abstract::Rng> getRng(const RunSettings& settings);
}


NAMESPACE_SPH_END
