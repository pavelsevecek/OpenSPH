#include "system/Factory.h"
#include "geometry/Domain.h"
#include "objects/finders/BruteForce.h"
#include "objects/finders/KdTree.h"
#include "physics/Eos.h"
#include "sph/initconds/InitConds.h"
#include "sph/timestepping/TimeStepping.h"

NAMESPACE_SPH_BEGIN

std::unique_ptr<Abstract::Eos> Factory::getEos(const Settings<BodySettingsIds>& settings) {
    const Optional<EosEnum> id = optionalCast<EosEnum>(settings.get<int>(BodySettingsIds::EOS));
    switch (id.get()) {
    case EosEnum::IDEAL_GAS:
        return std::make_unique<IdealGasEos>();
    case EosEnum::TILLOTSON:
        return std::make_unique<TillotsonEos>(settings);
    default:
        ASSERT(false);
        return nullptr;
    }
}

std::unique_ptr<Abstract::TimeStepping> Factory::getTimestepping(const Settings<GlobalSettingsIds>& settings,
                                                                 const std::shared_ptr<Storage>& storage) {
    const Optional<TimesteppingEnum> id =
        optionalCast<TimesteppingEnum>(settings.get<int>(GlobalSettingsIds::TIMESTEPPING_INTEGRATOR));
    ASSERT(id);
    switch (id.get()) {
    case TimesteppingEnum::EULER_EXPLICIT:
        return std::make_unique<EulerExplicit>(storage, settings);
    case TimesteppingEnum::PREDICTOR_CORRECTOR:
        return std::make_unique<PredictorCorrector>(storage, settings);
    case TimesteppingEnum::BULIRSCH_STOER:
        return std::make_unique<BulirschStoer>(storage, settings);
    default:
        ASSERT(false);
        return nullptr;
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
        return nullptr;
    }
}

std::unique_ptr<Abstract::Distribution> Factory::getDistribution(const Settings<BodySettingsIds>& settings) {
    const Optional<DistributionEnum> id =
        optionalCast<DistributionEnum>(settings.get<int>(BodySettingsIds::INITIAL_DISTRIBUTION));
    switch (id.get()) {
    case DistributionEnum::HEXAGONAL:
        return std::make_unique<HexagonalPacking>();
    case DistributionEnum::CUBIC:
        return std::make_unique<CubicPacking>();
    case DistributionEnum::RANDOM:
        return std::make_unique<RandomDistribution>();
    /*case DistributionEnum::DIEHL_ET_AL:
        return std::make_unique<DiehlDistribution>();*/
    default:
        ASSERT(false);
        return nullptr;
    }
}

LutKernel Factory::getKernel(const KernelEnum id) {
    switch (id) {
    case KernelEnum::CUBIC_SPLINE:
        return LutKernel(CubicSpline());
    default:
        ASSERT(false);
        throw std::exception();
    }
}

NAMESPACE_SPH_END
