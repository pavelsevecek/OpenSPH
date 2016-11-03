#include "system/Factory.h"
#include "geometry/Domain.h"
#include "objects/finders/BruteForce.h"
#include "objects/finders/KdTree.h"
#include "physics/Eos.h"
#include "physics/TimeStepping.h"
#include "sph/initconds/InitConds.h"

NAMESPACE_SPH_BEGIN

std::unique_ptr<Abstract::Eos> Factory::getEos(const EosEnum id, const Settings<BodySettingsIds>& settings) {
    switch (id) {
    case EosEnum::IDEAL_GAS:
        return std::make_unique<IdealGasEos>();
    case EosEnum::TILLOTSON:
        return std::make_unique<TillotsonEos>(settings);
    default:
        ASSERT(false);
    }
}

std::unique_ptr<Abstract::TimeStepping> Factory::getTimestepping(
    const TimesteppingEnum id,
    const std::shared_ptr<GenericStorage>& storage,
    const Settings<GlobalSettingsIds>& settings) {
    switch (id) {
    case TimesteppingEnum::EULER_EXPLICIT:
        return std::make_unique<EulerExplicit>(storage, settings);
    case TimesteppingEnum::PREDICTOR_CORRECTOR:
        return std::make_unique<PredictorCorrector>(storage, settings);
    case TimesteppingEnum::BULIRSCH_STOER:
        return std::make_unique<BulirschStoer>(storage, settings);
    default:
        ASSERT(false);
    }
}

std::unique_ptr<Abstract::Finder> Factory::getFinder(const FinderEnum id) {
    switch (id) {
    case FinderEnum::BRUTE_FORCE:
        return std::make_unique<BruteForceFinder>();
    case FinderEnum::KD_TREE:
        return std::make_unique<KdTree>();
    default:
        ASSERT(false);
    }
}

LutKernel Factory::getKernel(const KernelEnum id) {
    switch (id) {
    case KernelEnum::CUBIC_SPLINE:
        return LutKernel(CubicSpline());
    default:
        ASSERT(false);
    }
}

NAMESPACE_SPH_END
