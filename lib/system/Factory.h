#pragma once

/// \file Factory.h
/// \brief Creating code components based on values from settings.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/wrappers/ClonePtr.h"
#include "objects/wrappers/SharedPtr.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

// forward declarations
class IEos;
class IRheology;
class ISolver;
class IGravity;
class IDamage;
class IDistribution;
class IDomain;
class INeighbourFinder;
class ITimeStepping;
class ITimeStepCriterion;
class IBoundaryCondition;
class IMaterial;
class ILogger;
class IOutput;
class IRng;
class IEquationTerm;


class Storage;

/// Class providing a convenient way to construct objects from settings.
namespace Factory {
    AutoPtr<IEos> getEos(const BodySettings& settings);

    AutoPtr<IRheology> getRheology(const BodySettings& settings);

    AutoPtr<IDamage> getDamage(const BodySettings& settings);

    AutoPtr<IEquationTerm> getArtificialViscosity(const RunSettings& settings);

    AutoPtr<ITimeStepping> getTimeStepping(const RunSettings& settings, const SharedPtr<Storage>& storage);

    AutoPtr<ITimeStepCriterion> getTimeStepCriterion(const RunSettings& settings);

    AutoPtr<INeighbourFinder> getFinder(const RunSettings& settings);

    AutoPtr<IDistribution> getDistribution(const BodySettings& settings);

    AutoPtr<ISolver> getSolver(const RunSettings& settings);

    AutoPtr<IGravity> getGravity(const RunSettings& settings);

    AutoPtr<IBoundaryCondition> getBoundaryConditions(const RunSettings& settings, AutoPtr<IDomain>&& domain);

    AutoPtr<IDomain> getDomain(const RunSettings& settings);

    AutoPtr<IMaterial> getMaterial(const BodySettings& settings);

    AutoPtr<ILogger> getLogger(const RunSettings& settings);

    AutoPtr<IRng> getRng(const RunSettings& settings);
} // namespace Factory


NAMESPACE_SPH_END
