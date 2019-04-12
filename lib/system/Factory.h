#pragma once

/// \file Factory.h
/// \brief Creating code components based on values from settings.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/wrappers/SharedPtr.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

// forward declarations
class IEos;
class IRheology;
class ISolver;
class IGravity;
class ICollisionHandler;
class IOverlapHandler;
class IFractureModel;
class IDistribution;
class IDomain;
class ISymmetricFinder;
class ITimeStepping;
class ITimeStepCriterion;
class IBoundaryCondition;
class IMaterial;
class ILogger;
class IOutput;
class IScheduler;
class IRng;
class IEquationTerm;
class IScheduler;
class Storage;
enum class FinderFlag;
template <Size D>
class LutKernel;
class GravityLutKernel;


/// \brief Provides a convenient way to construct objects from settings.
namespace Factory {

/// \addtogroup Code components

AutoPtr<ILogger> getLogger(const RunSettings& settings);

AutoPtr<IOutput> getOutput(const RunSettings& settings);

AutoPtr<IRng> getRng(const RunSettings& settings);

AutoPtr<ISolver> getSolver(IScheduler& scheduler, const RunSettings& settings);

template <Size D>
LutKernel<D> getKernel(const RunSettings& settings);

GravityLutKernel getGravityKernel(const RunSettings& settings);

AutoPtr<IGravity> getGravity(const RunSettings& settings);

AutoPtr<IEquationTerm> getArtificialViscosity(const RunSettings& settings);

AutoPtr<ITimeStepping> getTimeStepping(const RunSettings& settings, const SharedPtr<Storage>& storage);

AutoPtr<ITimeStepCriterion> getTimeStepCriterion(const RunSettings& settings);

AutoPtr<ICollisionHandler> getCollisionHandler(const RunSettings& settings);

AutoPtr<IOverlapHandler> getOverlapHandler(const RunSettings& settings);

AutoPtr<IDomain> getDomain(const RunSettings& settings);

AutoPtr<IBoundaryCondition> getBoundaryConditions(const RunSettings& settings);

AutoPtr<ISymmetricFinder> getFinder(const RunSettings& settings);

SharedPtr<IScheduler> getScheduler(const RunSettings& settings);


/// \addtogroup Material components

AutoPtr<IMaterial> getMaterial(const BodySettings& settings);

AutoPtr<IDistribution> getDistribution(const BodySettings& settings);

AutoPtr<IEos> getEos(const BodySettings& settings);

AutoPtr<IRheology> getRheology(const BodySettings& settings);

AutoPtr<IFractureModel> getDamage(const BodySettings& settings);

} // namespace Factory


NAMESPACE_SPH_END
