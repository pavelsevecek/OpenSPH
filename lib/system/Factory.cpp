#include "system/Factory.h"
#include "geometry/Domain.h"
#include "io/Logger.h"
#include "math/rng/Rng.h"
#include "objects/finders/BruteForceFinder.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/Octree.h"
#include "objects/finders/Voxel.h"
#include "physics/Damage.h"
#include "physics/Eos.h"
#include "physics/Rheology.h"
#include "sph/Material.h"
#include "sph/boundary/Boundary.h"
#include "sph/equations/av/Balsara.h"
#include "sph/initial/Distribution.h"
#include "sph/solvers/ContinuitySolver.h"
#include "sph/solvers/DensityIndependentSolver.h"
#include "sph/solvers/SummationSolver.h"
#include "timestepping/TimeStepCriterion.h"
#include "timestepping/TimeStepping.h"

NAMESPACE_SPH_BEGIN

AutoPtr<Abstract::Eos> Factory::getEos(const BodySettings& settings) {
    const EosEnum id = settings.get<EosEnum>(BodySettingsId::EOS);
    switch (id) {
    case EosEnum::IDEAL_GAS:
        return makeAuto<IdealGasEos>(settings.get<Float>(BodySettingsId::ADIABATIC_INDEX));
    case EosEnum::TILLOTSON:
        return makeAuto<TillotsonEos>(settings);
    case EosEnum::MURNAGHAN:
        return makeAuto<MurnaghanEos>(settings);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<Abstract::Rheology> Factory::getRheology(const BodySettings& settings) {
    const YieldingEnum id = settings.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
    switch (id) {
    case YieldingEnum::NONE:
        return makeAuto<ElasticRheology>();
    case YieldingEnum::VON_MISES:
        return makeAuto<VonMisesRheology>(Factory::getDamage(settings));
    case YieldingEnum::DRUCKER_PRAGER:
        return makeAuto<DruckerPragerRheology>(Factory::getDamage(settings));
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<Abstract::Damage> Factory::getDamage(const BodySettings& settings) {
    const DamageEnum id = settings.get<DamageEnum>(BodySettingsId::RHEOLOGY_DAMAGE);
    switch (id) {
    case DamageEnum::NONE:
        return makeAuto<NullDamage>();
    case DamageEnum::SCALAR_GRADY_KIPP:
        /// \todo  where to get kernel radius from??
        return makeAuto<ScalarDamage>(2._f);
    case DamageEnum::TENSOR_GRADY_KIPP:
        return makeAuto<TensorDamage>();
    default:
        NOT_IMPLEMENTED;
    }
}

template <typename T>
AutoPtr<Abstract::EquationTerm> makeAV(const RunSettings& settings, const bool balsara) {
    if (balsara) {
        return makeAuto<BalsaraSwitch<T>>(settings);
    } else {
        return makeAuto<T>();
    }
}

AutoPtr<Abstract::EquationTerm> Factory::getArtificialViscosity(const RunSettings& settings) {
    const ArtificialViscosityEnum id = settings.get<ArtificialViscosityEnum>(RunSettingsId::SPH_AV_TYPE);
    const bool balsara = settings.get<bool>(RunSettingsId::SPH_AV_BALSARA);
    switch (id) {
    case ArtificialViscosityEnum::NONE:
        return nullptr;
    case ArtificialViscosityEnum::STANDARD:
        return makeAV<StandardAV>(settings, balsara);
    /*case ArtificialViscosityEnum::MORRIS_MONAGHAN:
        reutrn makeAV<MorrisMonaghanAV>();*/
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<Abstract::TimeStepping> Factory::getTimeStepping(const RunSettings& settings,
    const SharedPtr<Storage>& storage) {
    const TimesteppingEnum id = settings.get<TimesteppingEnum>(RunSettingsId::TIMESTEPPING_INTEGRATOR);
    switch (id) {
    case TimesteppingEnum::EULER_EXPLICIT:
        return makeAuto<EulerExplicit>(storage, settings);
    case TimesteppingEnum::PREDICTOR_CORRECTOR:
        return makeAuto<PredictorCorrector>(storage, settings);
    case TimesteppingEnum::BULIRSCH_STOER:
        return makeAuto<BulirschStoer>(storage, settings);
    case TimesteppingEnum::RUNGE_KUTTA:
        return makeAuto<RungeKutta>(storage, settings);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<Abstract::TimeStepCriterion> Factory::getTimeStepCriterion(const RunSettings& settings) {
    const Size flags = settings.get<int>(RunSettingsId::TIMESTEPPING_CRITERION);
    if (flags == 0) {
        // no criterion
        return nullptr;
    }
    switch (flags) {
    case Size(TimeStepCriterionEnum::COURANT):
        return makeAuto<CourantCriterion>(settings);
    case Size(TimeStepCriterionEnum::DERIVATIVES):
        return makeAuto<DerivativeCriterion>(settings);
    case Size(TimeStepCriterionEnum::ACCELERATION):
        return makeAuto<AccelerationCriterion>();
    default:
        ASSERT(!isPower2(flags)); // multiple criteria, assert in case we add another criterion
        return makeAuto<MultiCriterion>(settings);
    }
}

AutoPtr<Abstract::Finder> Factory::getFinder(const RunSettings& settings) {
    const FinderEnum id = settings.get<FinderEnum>(RunSettingsId::SPH_FINDER);
    switch (id) {
    case FinderEnum::BRUTE_FORCE:
        return makeAuto<BruteForceFinder>();
    case FinderEnum::KD_TREE:
        return makeAuto<KdTree>();
    case FinderEnum::OCTREE:
        return makeAuto<Octree>();
    case FinderEnum::VOXEL:
        return makeAuto<VoxelFinder>();
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<Abstract::Distribution> Factory::getDistribution(const BodySettings& settings) {
    const DistributionEnum id = settings.get<DistributionEnum>(BodySettingsId::INITIAL_DISTRIBUTION);
    const bool center = settings.get<bool>(BodySettingsId::CENTER_PARTICLES);
    const bool sort = settings.get<bool>(BodySettingsId::PARTICLE_SORTING);
    const bool sph5mode = settings.get<bool>(BodySettingsId::DISTRIBUTE_MODE_SPH5);
    switch (id) {
    case DistributionEnum::HEXAGONAL: {
        Flags<HexagonalPacking::Options> flags;
        flags.setIf(HexagonalPacking::Options::CENTER, center || sph5mode);
        flags.setIf(HexagonalPacking::Options::SORTED, sort);
        flags.setIf(HexagonalPacking::Options::SPH5_COMPATIBILITY, sph5mode);
        return makeAuto<HexagonalPacking>(flags);
    }
    case DistributionEnum::CUBIC:
        return makeAuto<CubicPacking>();
    case DistributionEnum::RANDOM:
        return makeAuto<RandomDistribution>();
    case DistributionEnum::DIEHL_ET_AL:
        return makeAuto<DiehlEtAlDistribution>([](const Vector&) { return 1._f; });
    case DistributionEnum::LINEAR:
        return makeAuto<LinearDistribution>();
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<Abstract::Solver> Factory::getSolver(const RunSettings& settings) {
    const SolverEnum id = settings.get<SolverEnum>(RunSettingsId::SOLVER_TYPE);
    switch (id) {
    case SolverEnum::CONTINUITY_SOLVER:
        return makeAuto<ContinuitySolver>(settings);
    case SolverEnum::SUMMATION_SOLVER:
        return makeAuto<SummationSolver>(settings);
    case SolverEnum::DENSITY_INDEPENDENT:
        return makeAuto<DensityIndependentSolver>(settings);
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<Abstract::Domain> Factory::getDomain(const RunSettings& settings) {
    const DomainEnum id = settings.get<DomainEnum>(RunSettingsId::DOMAIN_TYPE);
    const Vector center = settings.get<Vector>(RunSettingsId::DOMAIN_CENTER);
    switch (id) {
    case DomainEnum::NONE:
        return nullptr;
    case DomainEnum::BLOCK:
        return makeAuto<BlockDomain>(center, settings.get<Vector>(RunSettingsId::DOMAIN_SIZE));
    case DomainEnum::CYLINDER:
        return makeAuto<CylindricalDomain>(center,
            settings.get<Float>(RunSettingsId::DOMAIN_RADIUS),
            settings.get<Float>(RunSettingsId::DOMAIN_HEIGHT),
            true);
    case DomainEnum::SPHERICAL:
        return makeAuto<SphericalDomain>(center, settings.get<Float>(RunSettingsId::DOMAIN_RADIUS));
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<Abstract::BoundaryConditions> Factory::getBoundaryConditions(const RunSettings& settings,
    AutoPtr<Abstract::Domain>&& domain) {
    const BoundaryEnum id = settings.get<BoundaryEnum>(RunSettingsId::DOMAIN_BOUNDARY);
    switch (id) {
    case BoundaryEnum::NONE:
        return nullptr;
    case BoundaryEnum::GHOST_PARTICLES:
        ASSERT(domain != nullptr);
        return makeAuto<GhostParticles>(std::move(domain), settings);
    case BoundaryEnum::FROZEN_PARTICLES:
        if (domain) {
            const Float radius = settings.get<Float>(RunSettingsId::DOMAIN_FROZEN_DIST);
            return makeAuto<FrozenParticles>(std::move(domain), radius);
        } else {
            return makeAuto<FrozenParticles>();
        }
    case BoundaryEnum::WIND_TUNNEL: {
        ASSERT(domain != nullptr);
        const Float radius = settings.get<Float>(RunSettingsId::DOMAIN_FROZEN_DIST);
        return makeAuto<WindTunnel>(std::move(domain), radius);
    }
    case BoundaryEnum::PROJECT_1D: {
        ASSERT(domain != nullptr);
        const Box box = domain->getBoundingBox();
        return makeAuto<Projection1D>(Range(box.lower()[X], box.upper()[X]));
    }
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<Abstract::Material> Factory::getMaterial(const BodySettings& settings) {
    const YieldingEnum yieldId = settings.get<YieldingEnum>(BodySettingsId::RHEOLOGY_YIELDING);
    const EosEnum eosId = settings.get<EosEnum>(BodySettingsId::EOS);
    switch (yieldId) {
    case YieldingEnum::DRUCKER_PRAGER:
    case YieldingEnum::VON_MISES:
        return makeAuto<SolidMaterial>(settings, Factory::getEos(settings), Factory::getRheology(settings));
    case YieldingEnum::NONE:
        switch (eosId) {
        case EosEnum::NONE:
            return makeAuto<NullMaterial>(settings);
        default:
            return makeAuto<EosMaterial>(settings, Factory::getEos(settings));
        }

    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<Abstract::Logger> Factory::getLogger(const RunSettings& settings) {
    const LoggerEnum id = settings.get<LoggerEnum>(RunSettingsId::RUN_LOGGER);
    switch (id) {
    case LoggerEnum::NONE:
        return makeAuto<DummyLogger>();
    case LoggerEnum::STD_OUT:
        return makeAuto<StdOutLogger>();
    case LoggerEnum::FILE: {
        const Path path(settings.get<std::string>(RunSettingsId::RUN_LOGGER_FILE));
        return makeAuto<FileLogger>(path);
    }
    default:
        NOT_IMPLEMENTED;
    }
}

AutoPtr<Abstract::Rng> Factory::getRng(const RunSettings& settings) {
    const RngEnum id = settings.get<RngEnum>(RunSettingsId::RUN_RNG);
    const int seed = settings.get<int>(RunSettingsId::RUN_RNG_SEED);
    switch (id) {
    case RngEnum::UNIFORM:
        return makeAuto<RngWrapper<UniformRng>>(seed);
    case RngEnum::HALTON:
        return makeAuto<RngWrapper<HaltonQrng>>();
    case RngEnum::BENZ_ASPHAUG:
        return makeAuto<RngWrapper<BenzAsphaugRng>>(seed);
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
