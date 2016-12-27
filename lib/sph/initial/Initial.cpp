#include "sph/initial/Initial.h"
#include "geometry/Domain.h"
#include "physics/Eos.h"
#include "solvers/AbstractSolver.h"
#include "solvers/SolverFactory.h"
#include "sph/initial/Distribution.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "system/Factory.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

InitialConditions::InitialConditions(const std::shared_ptr<Storage> storage,
    const GlobalSettings& globalSettings)
    : storage(storage)
    , solver(getSolver(globalSettings)) {}

InitialConditions::~InitialConditions() = default;

void InitialConditions::addBody(Abstract::Domain& domain, BodySettings& bodySettings) {
    Storage body(bodySettings);
    int N; // Final number of particles
    PROFILE_SCOPE("InitialConditions::addBody");

    // generate particle positions
    std::unique_ptr<Abstract::Distribution> distribution = Factory::getDistribution(bodySettings);

    const int n = bodySettings.get<int>(BodySettingsIds::PARTICLE_COUNT);
    // Generate positions of particles
    Array<Vector> r = distribution->generate(n, domain);
    N = r.size();
    body.emplace<Vector, OrderEnum::SECOND_ORDER>(QuantityKey::POSITIONS, std::move(r));
    ASSERT(N > 0);

    // Set masses of particles, assuming all particles have the same mass
    /// \todo this has to be generalized when using nonuniform particle destribution
    const Float rho0 = bodySettings.get<Float>(BodySettingsIds::DENSITY);
    const Float totalM = domain.getVolume() * rho0; // m = rho * V
    ASSERT(totalM > 0._f);
    body.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::MASSES, totalM / N);

    solver->initialize(body, bodySettings);
    if (storage->getQuantityCnt() == 0) {
        // this is a first body, simply assign storage
        *storage = std::move(body);
    } else {
        // more than one body, merge storages
        storage->merge(std::move(body));
    }
}


NAMESPACE_SPH_END
