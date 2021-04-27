#pragma once

/// \file Factory.h
/// \brief Creating code components based on values from settings.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/wrappers/Function.h"
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
class ILogWriter;
class IOutput;
class IInput;
class IScheduler;
class IRng;
class IEquationTerm;
class IScheduler;
class IUvMapping;
class Storage;
class EquationHolder;
enum class FinderFlag;
template <Size D>
class LutKernel;
class GravityLutKernel;


/// \brief Provides a convenient way to construct objects from settings.
namespace Factory {

/// \addtogroup Code components

AutoPtr<ILogger> getLogger(const RunSettings& settings);

AutoPtr<ILogWriter> getLogWriter(SharedPtr<ILogger> logger, const RunSettings& settings);

AutoPtr<IOutput> getOutput(const RunSettings& settings);

// deduces format from path extension
AutoPtr<IInput> getInput(const Path& path);

AutoPtr<IRng> getRng(const RunSettings& settings);

AutoPtr<ISolver> getSolver(IScheduler& scheduler, const RunSettings& settings);

AutoPtr<ISolver> getSolver(IScheduler& scheduler,
    const RunSettings& settings,
    AutoPtr<IBoundaryCondition>&& bc);

AutoPtr<ISolver> getSolver(IScheduler& scheduler,
    const RunSettings& settings,
    AutoPtr<IBoundaryCondition>&& bc,
    const EquationHolder& additionalTerms);

template <Size D>
LutKernel<D> getKernel(const RunSettings& settings);

GravityLutKernel getGravityKernel(const RunSettings& settings);

AutoPtr<IGravity> getGravity(const RunSettings& settings);

AutoPtr<IEquationTerm> getArtificialViscosity(const RunSettings& settings);

AutoPtr<ITimeStepping> getTimeStepping(const RunSettings& settings, const SharedPtr<Storage>& storage);

AutoPtr<ITimeStepCriterion> getTimeStepCriterion(const RunSettings& settings);

AutoPtr<ICollisionHandler> getCollisionHandler(const RunSettings& settings);

AutoPtr<IOverlapHandler> getOverlapHandler(const RunSettings& settings);

/// \todo split IDomain into IComputationalDomain and IShape (or IBodyShape)
AutoPtr<IDomain> getDomain(const RunSettings& settings);

AutoPtr<IDomain> getDomain(const BodySettings& settings);

AutoPtr<IBoundaryCondition> getBoundaryConditions(const RunSettings& settings, SharedPtr<IDomain> domain);

AutoPtr<IBoundaryCondition> getBoundaryConditions(const RunSettings& settings);

AutoPtr<ISymmetricFinder> getFinder(const RunSettings& settings);

SharedPtr<IScheduler> getScheduler(const RunSettings& settings = RunSettings::getDefaults());

AutoPtr<IUvMapping> getUvMapping(const RunSettings& settings);

/// \addtogroup Material components

AutoPtr<IMaterial> getMaterial(const BodySettings& settings);

AutoPtr<IDistribution> getDistribution(const BodySettings& settings,
    Function<bool(Float)> progressCallback = nullptr);

AutoPtr<IEos> getEos(const BodySettings& settings);

AutoPtr<IRheology> getRheology(const BodySettings& settings);

AutoPtr<IFractureModel> getDamage(const BodySettings& settings);

} // namespace Factory


NAMESPACE_SPH_END
