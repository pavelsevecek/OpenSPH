#include "sph/initial/Initial.h"
#include "geometry/Domain.h"
#include "physics/Eos.h"
#include "solvers/AbstractSolver.h"
#include "solvers/SolverFactory.h"
#include "sph/initial/Distribution.h"
#include "storage/Quantity.h"
#include "storage/Storage.h"
#include "system/Factory.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

InitialConditions::InitialConditions(const GlobalSettings& globalSettings)
    : solver(getSolver(globalSettings)) {}


void InitialConditions::addBody(Abstract::Domain& domain, BodySettings& bodySettings) {
    Storage storage(bodySettings);
    int N; // Final number of particles
    PROFILE_SCOPE("InitialConditions::addBody");

    // generate particle positions
    std::unique_ptr<Abstract::Distribution> distribution = Factory::getDistribution(bodySettings);

    const int n = bodySettings.get<int>(BodySettingsIds::PARTICLE_COUNT);
    // Generate positions of particles
    Array<Vector> r = distribution->generate(n, domain);
    N = r.size();
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(QuantityKey::R, std::move(r));
    ASSERT(N > 0);

    // Set masses of particles, assuming all particles have the same mass
    /// \todo this has to be generalized when using nonuniform particle destribution
    const Float rho0 = bodySettings.get<Float>(BodySettingsIds::DENSITY);
    const Float totalM = domain.getVolume() * rho0; // m = rho * V
    ASSERT(totalM > 0._f);
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityKey::M, totalM / N);

    solver->initialize(storage, bodySettings);
}


NAMESPACE_SPH_END
