#include "sph/initial/Initial.h"
#include "math/rng/Rng.h"
#include "objects/geometry/Domain.h"
#include "physics/Eos.h"
#include "physics/Integrals.h"
#include "quantities/IMaterial.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/initial/Distribution.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

BodyView::BodyView(Storage& storage, const Size bodyIndex)
    : storage(storage)
    , bodyIndex(bodyIndex) {}

BodyView& BodyView::addVelocity(const Vector& velocity) {
    ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITIONS);
    // Body created using InitialConditions always has a material
    MaterialView material = storage.getMaterial(bodyIndex);
    for (Size i : material.sequence()) {
        v[i] += velocity;
    }
    return *this;
}

BodyView& BodyView::addRotation(const Vector& omega, const Vector& origin) {
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    MaterialView material = storage.getMaterial(bodyIndex);
    for (Size i : material.sequence()) {
        v[i] += cross(r[i] - origin, omega);
    }
    return *this;
}

Vector BodyView::getOrigin(const RotationOrigin origin) const {
    switch (origin) {
    case RotationOrigin::FRAME_ORIGIN:
        return Vector(0._f);
    case RotationOrigin::CENTER_OF_MASS:
        return CenterOfMass(bodyIndex).evaluate(storage);
    default:
        NOT_IMPLEMENTED;
    }
}

BodyView& BodyView::addRotation(const Vector& omega, const RotationOrigin origin) {
    return this->addRotation(omega, this->getOrigin(origin));
}


/// Solver taking a reference of another solver and forwarding all function, used to convert owning AutoPtr to
/// non-owning pointer.
class ForwardingSolver : public ISolver {
private:
    ISolver& solver;

public:
    ForwardingSolver(ISolver& solver)
        : solver(solver) {}

    virtual void integrate(Storage& storage, Statistics& stats) override {
        solver.integrate(storage, stats);
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        solver.create(storage, material);
    }
};

InitialConditions::InitialConditions(Storage& storage, ISolver& solver, const RunSettings& settings)
    : storage(storage)
    , solver(makeAuto<ForwardingSolver>(solver)) {
    this->createCommon(settings);
}

InitialConditions::InitialConditions(Storage& storage, const RunSettings& settings)
    : storage(storage)
    , solver(Factory::getSolver(settings)) {
    this->createCommon(settings);
}

InitialConditions::~InitialConditions() {
    this->finalize();
}

void InitialConditions::finalize() {
    if (finalization.storeInitialPositions) {
        Array<Vector> cloned = storage.getValue<Vector>(QuantityId::POSITIONS).clone();
        storage.insert<Vector>(QuantityId::INITIAL_POSITION, OrderEnum::ZERO, std::move(cloned));
        finalization.storeInitialPositions = false;
    }
}

void InitialConditions::createCommon(const RunSettings& settings) {
    /// \todo more general rng (from settings)
    context.rng = makeAuto<RngWrapper<BenzAsphaugRng>>(1234);
    finalization.storeInitialPositions = settings.get<bool>(RunSettingsId::OUTPUT_SAVE_INITIAL_POSITION);
}

BodyView InitialConditions::addBody(const IDomain& domain, const BodySettings& settings) {
    AutoPtr<IMaterial> material = Factory::getMaterial(settings);
    return this->addBody(domain, std::move(material));
}

BodyView InitialConditions::addBody(const IDomain& domain, AutoPtr<IMaterial>&& material) {
    IMaterial& mat = *material; // get reference before moving the pointer
    Storage body(std::move(material));

    PROFILE_SCOPE("InitialConditions::addBody");
    AutoPtr<IDistribution> distribution = Factory::getDistribution(mat.getParams());
    const Size n = mat.getParam<int>(BodySettingsId::PARTICLE_COUNT);

    // Generate positions of particles
    Array<Vector> positions = distribution->generate(n, domain);
    ASSERT(positions.size() > 0);
    body.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND, std::move(positions));

    this->setQuantities(body, mat, domain.getVolume());
    storage.merge(std::move(body));
    return BodyView(storage, bodyIndex++);
}

InitialConditions::BodySetup::BodySetup(AutoPtr<IDomain>&& domain, AutoPtr<IMaterial>&& material)
    : domain(std::move(domain))
    , material(std::move(material)) {}

InitialConditions::BodySetup::BodySetup(AutoPtr<IDomain>&& domain, const BodySettings& settings)
    : domain(std::move(domain))
    , material(Factory::getMaterial(settings)) {}

InitialConditions::BodySetup::BodySetup(BodySetup&& other)
    : domain(std::move(other.domain))
    , material(std::move(other.material)) {}

InitialConditions::BodySetup::BodySetup() = default;

InitialConditions::BodySetup::~BodySetup() = default;


Array<BodyView> InitialConditions::addHeterogeneousBody(BodySetup&& environment,
    ArrayView<BodySetup> bodies) {
    PROFILE_SCOPE("InitialConditions::addHeterogeneousBody");
    AutoPtr<IDistribution> distribution = Factory::getDistribution(environment.material->getParams());
    const Size n = environment.material->getParam<int>(BodySettingsId::PARTICLE_COUNT);

    // Generate positions of ALL particles
    Array<Vector> positions = distribution->generate(n, *environment.domain);
    // Create particle storage per body
    Storage environmentStorage(std::move(environment.material));
    Array<Storage> bodyStorages;
    for (BodySetup& body : bodies) {
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

    // Return value
    Array<BodyView> views;

    // Initialize storages
    Float environmentVolume = environment.domain->getVolume();
    for (Size i = 0; i < bodyStorages.size(); ++i) {
        bodyStorages[i].insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND, std::move(pos_bodies[i]));
        const Float volume = bodies[i].domain->getVolume();
        setQuantities(bodyStorages[i], bodyStorages[i].getMaterial(0), volume);
        views.emplaceBack(BodyView(storage, bodyIndex++));
        environmentVolume -= volume;
    }
    ASSERT(environmentVolume >= 0._f);
    environmentStorage.insert<Vector>(QuantityId::POSITIONS, OrderEnum::SECOND, std::move(pos_env));
    setQuantities(environmentStorage, environmentStorage.getMaterial(0), environmentVolume);
    views.emplaceBack(BodyView(storage, bodyIndex++));

    // merge all storages
    storage.merge(std::move(environmentStorage));
    for (Storage& body : bodyStorages) {
        storage.merge(std::move(body));
    }

    return views;
}

void InitialConditions::setQuantities(Storage& storage, IMaterial& material, const Float volume) {
    // Set masses of particles, assuming all particles have the same mass
    /// \todo this has to be generalized when using nonuniform particle destribution
    const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
    const Float totalM = volume * rho0; // m = rho * V
    ASSERT(totalM > 0._f);
    storage.insert<Float>(QuantityId::MASSES, OrderEnum::ZERO, totalM / storage.getParticleCnt());

    // Mark particles of this body
    storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, bodyIndex);

    // Initialize all quantities needed by the solver
    solver->create(storage, material);

    // Initialize material (we need density and energy for that)
    material.create(storage, context);
}


NAMESPACE_SPH_END
