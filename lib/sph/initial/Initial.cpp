#include "sph/initial/Initial.h"
#include "geometry/Domain.h"
#include "math/rng/Rng.h"
#include "physics/Eos.h"
#include "quantities/AbstractMaterial.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/initial/Distribution.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "timestepping/AbstractSolver.h"

NAMESPACE_SPH_BEGIN

InitialConditions::InitialConditions(Storage& storage,
    AutoPtr<Abstract::Solver>&& solver,
    const RunSettings& settings)
    : storage(storage)
    , solver(std::move(solver)) {
    context.rng = Factory::getRng(settings);
}

InitialConditions::InitialConditions(Storage& storage, const RunSettings& settings)
    : InitialConditions(storage, Factory::getSolver(settings), settings) {}

InitialConditions::~InitialConditions() = default;


void InitialConditions::addBody(const Abstract::Domain& domain,
    const BodySettings& settings,
    const Vector& velocity,
    const Vector& angularVelocity) {
    AutoPtr<Abstract::Material> material = Factory::getMaterial(settings);
    this->addBody(domain, std::move(material), velocity, angularVelocity);
}

void InitialConditions::addBody(const Abstract::Domain& domain,
    AutoPtr<Abstract::Material>&& material,
    const Vector& velocity,
    const Vector& angularVelocity) {

    Abstract::Material& mat = *material; // get reference before moving the pointer
    Storage body(std::move(material));

    PROFILE_SCOPE("InitialConditions::addBody");
    AutoPtr<Abstract::Distribution> distribution = Factory::getDistribution(mat.getParams());
    const Size n = mat.getParam<int>(BodySettingsId::PARTICLE_COUNT);

    // Generate positions of particles
    Array<Vector> positions = distribution->generate(n, domain);
    ASSERT(positions.size() > 0);
    body.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND, std::move(positions));

    setQuantities(body, mat, domain.getVolume(), velocity, angularVelocity);

    if (storage.getQuantityCnt() == 0) {
        // this is a first body, simply assign storage
        storage = std::move(body);
    } else {
        // more than one body, merge storages
        storage.merge(std::move(body));
    }
}

InitialConditions::Body::Body(AutoPtr<Abstract::Domain>&& domain,
    AutoPtr<Abstract::Material>&& material,
    const Vector& velocity,
    const Vector& angularVelocity)
    : domain(std::move(domain))
    , material(std::move(material))
    , velocity(velocity)
    , angularVelocity(angularVelocity) {}

InitialConditions::Body::Body(AutoPtr<Abstract::Domain>&& domain,
    const BodySettings& settings,
    const Vector& velocity,
    const Vector& angularVelocity)
    : domain(std::move(domain))
    , material(Factory::getMaterial(settings))
    , velocity(velocity)
    , angularVelocity(angularVelocity) {}

InitialConditions::Body::Body(Body&& other)
    : domain(std::move(other.domain))
    , material(std::move(other.material))
    , velocity(other.velocity)
    , angularVelocity(other.angularVelocity) {}

InitialConditions::Body::Body() = default;

InitialConditions::Body::~Body() = default;

void InitialConditions::addHeterogeneousBody(Body&& environment, ArrayView<Body> bodies) {
    PROFILE_SCOPE("InitialConditions::addHeterogeneousBody");
    AutoPtr<Abstract::Distribution> distribution =
        Factory::getDistribution(environment.material->getParams());
    const Size n = environment.material->getParam<int>(BodySettingsId::PARTICLE_COUNT);

    // Generate positions of ALL particles
    Array<Vector> positions = distribution->generate(n, *environment.domain);
    // Create particle storage per body
    Storage environmentStorage(std::move(environment.material));
    Array<Storage> bodyStorages;
    for (Body& body : bodies) {
        bodyStorages.push(Storage(std::move(body.material)));
    }
    // Assign particles to bodies
    Array<Vector> pos_env;
    Array<Array<Vector>> pos_bodies(bodies.size());
    auto assign = [&](const Vector& p) {
        for (Size i = 0; i < bodies.size(); ++i) {
            if (bodies[i].domain->isInside(p)) {
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
    Float environmentVolume = environment.domain->getVolume();
    for (Size i = 0; i < bodyStorages.size(); ++i) {
        bodyStorages[i].insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND, std::move(pos_bodies[i]));
        const Float volume = bodies[i].domain->getVolume();
        setQuantities(bodyStorages[i],
            bodyStorages[i].getMaterial(0),
            volume,
            bodies[i].velocity,
            bodies[i].angularVelocity);
        environmentVolume -= volume;
    }
    ASSERT(environmentVolume >= 0._f);
    environmentStorage.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND, std::move(pos_env));
    setQuantities(environmentStorage,
        environmentStorage.getMaterial(0),
        environmentVolume,
        environment.velocity,
        environment.angularVelocity);
    // merge all storages
    if (storage.getQuantityCnt() == 0) {
        // this is a first body, simply assign storage
        storage = std::move(environmentStorage);
    } else {
        // more than one body, merge storages
        storage.merge(std::move(environmentStorage));
    }
    for (Storage& body : bodyStorages) {
        storage.merge(std::move(body));
    }
}

void InitialConditions::setQuantities(Storage& storage,
    Abstract::Material& material,
    const Float volume,
    const Vector& velocity,
    const Vector& angularVelocity) {
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        v[i] += velocity + cross(r[i], angularVelocity);
    }
    // Set masses of particles, assuming all particles have the same mass
    /// \todo this has to be generalized when using nonuniform particle destribution
    const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
    const Float totalM = volume * rho0; // m = rho * V
    ASSERT(totalM > 0._f);
    storage.insert<Float>(QuantityId::MASSES, OrderEnum::ZERO, totalM / r.size());

    // Mark particles of this body
    storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, bodyIndex);
    bodyIndex++;

    // Initialize all quantities needed by the solver
    solver->create(storage, material);

    // Initialize material (we need density and energy for that)
    material.create(storage, initialContext);
}


NAMESPACE_SPH_END
