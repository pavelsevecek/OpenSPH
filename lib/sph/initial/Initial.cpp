#include "sph/initial/Initial.h"
#include "geometry/Domain.h"
#include "math/rng/Rng.h"
#include "physics/Eos.h"
#include "quantities/AbstractMaterial.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "solvers/AbstractSolver.h"
#include "solvers/SolverFactory.h"
#include "sph/initial/Distribution.h"
#include "system/Factory.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

InitialConditions::InitialConditions(const std::shared_ptr<Storage> storage,
    const GlobalSettings& globalSettings)
    : storage(storage) {
    ASSERT(storage != nullptr);
    ASSERT(solver != nullptr);

    context.rng = Factory::getRng(globalSettings);
}

InitialConditions::~InitialConditions() = default;


void InitialConditions::addBody(const Abstract::Domain& domain,
    const BodySettings& settings,
    const Vector& velocity,
    const Vector& angularVelocity) {
    std::unique_ptr<Abstract::Material> material = Factory::getMaterial(settings);
    this->addBody(domain, settings, std::move(material), velocity, angularVelocity);
}

void InitialConditions::addBody(const Abstract::Domain& domain,
    const BodySettings& settings,
    std::unique_ptr<Abstract::Material>&& material,
    const Vector& velocity,
    const Vector& angularVelocity) {

    Storage body(std::move(material));
    PROFILE_SCOPE("InitialConditions::addBody");
    std::unique_ptr<Abstract::Distribution> distribution = Factory::getDistribution(settings);
    const Size n = settings.get<int>(BodySettingsIds::PARTICLE_COUNT);

    // Generate positions of particles
    Array<Vector> positions = distribution->generate(n, domain);
    ASSERT(positions.size() > 0);
    body.insert<Vector>(QuantityIds::POSITIONS, OrderEnum::SECOND, std::move(positions));

    setQuantities(body, settings, domain.getVolume(), velocity, angularVelocity);

    if (storage->getQuantityCnt() == 0) {
        // this is a first body, simply assign storage
        *storage = std::move(body);
    } else {
        // more than one body, merge storages
        storage->merge(std::move(body));
    }
}

void InitialConditions::addHeterogeneousBody(const Body& environment, ArrayView<const Body> bodies) {
    PROFILE_SCOPE("InitialConditions::addHeterogeneousBody");
    std::unique_ptr<Abstract::Distribution> distribution = Factory::getDistribution(environment.settings);
    const Size n = environment.settings.get<int>(BodySettingsIds::PARTICLE_COUNT);

    // Generate positions of ALL particles
    Array<Vector> positions = distribution->generate(n, environment.domain);
    // Create particle storage per body
    Storage environmentStorage(Factory::getMaterial(environment.settings));
    Array<Storage> bodyStorages;
    for (const Body& body : bodies) {
        bodyStorages.push(Storage(Factory::getMaterial(body.settings)));
    }
    // Assign particles to bodies
    Array<Vector> pos_env;
    Array<Array<Vector>> pos_bodies(bodies.size());
    auto assign = [&](const Vector& p) {
        for (Size i = 0; i < bodies.size(); ++i) {
            if (bodies[i].domain.isInside(p)) {
                pos_bodies[i].push(p);
                return true;
            }
        }
        return false;
    };
    for (const Vector& p : positions) {
        if (!assign(p)) {
            pos_env.push(p);
        }
    }

    // Initialize storages
    Float environmentVolume = environment.domain.getVolume();
    for (Size i = 0; i < bodyStorages.size(); ++i) {
        bodyStorages[i].insert<Vector>(QuantityIds::POSITIONS, OrderEnum::SECOND, std::move(pos_bodies[i]));
        const Float volume = bodies[i].domain.getVolume();
        setQuantities(
            bodyStorages[i], bodies[i].settings, volume, bodies[i].velocity, bodies[i].angularVelocity);
        environmentVolume -= volume;
    }
    ASSERT(environmentVolume >= 0._f);
    environmentStorage.insert<Vector>(QuantityIds::POSITIONS, OrderEnum::SECOND, std::move(pos_env));
    setQuantities(environmentStorage,
        environment.settings,
        environmentVolume,
        environment.velocity,
        environment.angularVelocity);
    // merge all storages
    if (storage->getQuantityCnt() == 0) {
        // this is a first body, simply assign storage
        *storage = std::move(environmentStorage);
    } else {
        // more than one body, merge storages
        storage->merge(std::move(environmentStorage));
    }
    for (Storage& body : bodyStorages) {
        storage->merge(std::move(body));
    }
}

void InitialConditions::setQuantities(Storage& storage,
    const BodySettings& settings,
    const Float volume,
    const Vector& velocity,
    const Vector& angularVelocity) {
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        v[i] += velocity + cross(r[i], angularVelocity);
    }
    // Set masses of particles, assuming all particles have the same mass
    /// \todo this has to be generalized when using nonuniform particle destribution
    const Float rho0 = settings.get<Float>(BodySettingsIds::DENSITY);
    const Float totalM = volume * rho0; // m = rho * V
    ASSERT(totalM > 0._f);
    storage.insert<Float>(QuantityIds::MASSES, OrderEnum::ZERO, totalM / r.size());

    // Mark particles of this body
    storage.insert<Size>(QuantityIds::FLAG, OrderEnum::ZERO, bodyIndex);
    bodyIndex++;

    // Initialize all quantities needed by the solver
    solver->create(storage, settings);
}


NAMESPACE_SPH_END
