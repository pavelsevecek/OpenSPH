#include "sph/initial/Initial.h"
#include "math/rng/VectorRng.h"
#include "objects/finders/NeighborFinder.h"
#include "objects/geometry/Domain.h"
#include "objects/geometry/Sphere.h"
#include "physics/Eos.h"
#include "physics/Integrals.h"
#include "quantities/IMaterial.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/initial/Distribution.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "thread/Scheduler.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

BodyView::BodyView(Storage& storage, const Size bodyIndex)
    : storage(&storage)
    , bodyIndex(bodyIndex) {}

BodyView& BodyView::displace(const Vector& dr) {
    // manually clear the h component to make sure we are not modifying smoothing length
    Vector actDr = dr;
    actDr[H] = 0.f;

    ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
    ArrayView<Size> flag = storage->getValue<Size>(QuantityId::FLAG);
    for (Size i = 0; i < r.size(); ++i) {
        if (flag[i] == bodyIndex) {
            r[i] += actDr;
        }
    }
    return *this;
}

BodyView& BodyView::addVelocity(const Vector& velocity) {
    ArrayView<Vector> v = storage->getDt<Vector>(QuantityId::POSITION);
    ArrayView<Size> flag = storage->getValue<Size>(QuantityId::FLAG);
    for (Size i = 0; i < v.size(); ++i) {
        if (flag[i] == bodyIndex) {
            v[i] += velocity;
        }
    }
    return *this;
}

BodyView& BodyView::addRotation(const Vector& omega, const Vector& origin) {
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage->getAll<Vector>(QuantityId::POSITION);
    ArrayView<Size> flag = storage->getValue<Size>(QuantityId::FLAG);
    for (Size i = 0; i < r.size(); ++i) {
        if (flag[i] == bodyIndex) {
            v[i] += cross(omega, r[i] - origin);
        }
    }
    return *this;
}

Vector BodyView::getOrigin(const RotationOrigin origin) const {
    switch (origin) {
    case RotationOrigin::FRAME_ORIGIN:
        return Vector(0._f);
    case RotationOrigin::CENTER_OF_MASS:
        return CenterOfMass(bodyIndex).evaluate(*storage);
    default:
        NOT_IMPLEMENTED;
    }
}

BodyView& BodyView::addRotation(const Vector& omega, const RotationOrigin origin) {
    return this->addRotation(omega, this->getOrigin(origin));
}

InitialConditions::InitialConditions(const RunSettings& settings)
    : context(settings) {}

InitialConditions::~InitialConditions() = default;

BodyView InitialConditions::addMonolithicBody(Storage& storage, const BodySettings& body) {
    AutoPtr<IDomain> domain = Factory::getDomain(body);
    return this->addMonolithicBody(storage, *domain, body);
}

BodyView InitialConditions::addMonolithicBody(Storage& storage,
    const IDomain& domain,
    const BodySettings& body) {
    AutoPtr<IMaterial> material = Factory::getMaterial(body);
    return this->addMonolithicBody(storage, domain, std::move(material));
}

BodyView InitialConditions::addMonolithicBody(Storage& storage,
    const IDomain& domain,
    SharedPtr<IMaterial> material) {
    AutoPtr<IDistribution> distribution = Factory::getDistribution(material->getParams());
    return this->addMonolithicBody(storage, domain, std::move(material), *distribution);
}

BodyView InitialConditions::addMonolithicBody(Storage& storage,
    const IDomain& domain,
    SharedPtr<IMaterial> material,
    const IDistribution& distribution) {
    Storage body(material);

    PROFILE_SCOPE("InitialConditions::addBody");
    const Size n = material->getParam<int>(BodySettingsId::PARTICLE_COUNT);

    // Generate positions of particles
    Array<Vector> positions = distribution.generate(*context.scheduler, n, domain);
    SPH_ASSERT(positions.size() > 0);
    body.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(positions));

    this->setQuantities(body, *material, domain.getVolume());
    storage.merge(std::move(body));
    const Size particleCnt = storage.getParticleCnt();

    /// \todo refactor
    storage.propagate([&particleCnt, &storage](Storage& act) { //
        SPH_ASSERT(&act != &storage);
        (void)storage;
        act.resize(particleCnt, Storage::ResizeFlag::KEEP_EMPTY_UNCHANGED);
    });
    return BodyView(storage, bodyIndex++);
}

InitialConditions::BodySetup::BodySetup(SharedPtr<IDomain> domain, SharedPtr<IMaterial> material)
    : domain(domain)
    , material(material) {}

InitialConditions::BodySetup::BodySetup(SharedPtr<IDomain> domain, const BodySettings& body)
    : domain(domain)
    , material(Factory::getMaterial(body)) {}

InitialConditions::BodySetup::BodySetup() = default;

InitialConditions::BodySetup::~BodySetup() = default;


BodyView InitialConditions::addHeterogeneousBody(Storage& storage,
    const BodySetup& environment,
    ArrayView<const BodySetup> bodies) {
    AutoPtr<IDistribution> distribution = Factory::getDistribution(environment.material->getParams());
    const Size n = environment.material->getParam<int>(BodySettingsId::PARTICLE_COUNT);

    // Generate positions of ALL particles
    Array<Vector> positions = distribution->generate(*context.scheduler, n, *environment.domain);
    // Create particle storage per body
    Storage enviroStorage(environment.material);
    Array<Storage> bodyStorages;
    for (const BodySetup& body : bodies) {
        bodyStorages.push(Storage(body.material));
    }
    // Assign particles to bodies
    Array<Vector> pos_env;
    Array<Array<Vector>> pos_bodies(bodies.size());
    auto assign = [&](const Vector& p) {
        for (Size i = 0; i < bodies.size(); ++i) {
            if (bodies[i].domain->contains(p)) {
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
    Float environVolume = environment.domain->getVolume();
    for (Size i = 0; i < bodyStorages.size(); ++i) {
        bodyStorages[i].insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(pos_bodies[i]));
        const Float volume = bodies[i].domain->getVolume();
        this->setQuantities(bodyStorages[i], bodyStorages[i].getMaterial(0), volume);
        environVolume -= volume;
    }
    SPH_ASSERT(environVolume >= 0._f);
    enviroStorage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(pos_env));
    this->setQuantities(enviroStorage, enviroStorage.getMaterial(0), environVolume);

    // merge all storages
    storage.merge(std::move(enviroStorage));
    for (Storage& body : bodyStorages) {
        storage.merge(std::move(body));
    }

    return BodyView(storage, bodyIndex++);
}

void InitialConditions::addRubblePileBody(Storage& storage,
    const IDomain& domain,
    const PowerLawSfd& sfd,
    const BodySettings& bodySettings) {
    const Size n = bodySettings.get<int>(BodySettingsId::PARTICLE_COUNT);
    const Size minN = bodySettings.get<int>(BodySettingsId::MIN_PARTICLE_COUNT);

    SPH_ASSERT(context.rng);
    VectorRng<IRng&> rng(*context.rng);

    // stack of generated spheres, to check for overlap
    Array<Sphere> spheres;

    // generate the particles that will be eventually turned into spheres
    AutoPtr<IDistribution> distribution = Factory::getDistribution(bodySettings);
    Array<Vector> positions = distribution->generate(*context.scheduler, n, domain);
    SharedPtr<IMaterial> material = Factory::getMaterial(bodySettings);

    // counter used to exit the loop (when no more spheres can be generated)
    Size bailoutCounter = 0;
    constexpr Size bailoutTarget = 1000;

    while (bailoutCounter < bailoutTarget) {

        Vector center;
        Float radius;
        while (bailoutCounter < bailoutTarget) {

            // generate a center of the sphere
            const Box box = domain.getBoundingBox();
            center = box.lower() + rng() * box.size();
            if (!domain.contains(center)) {
                // outside of the domain, reject (do not increase the bailoutCounter here)
                continue;
            }

            // generate a radius
            radius = sfd(rng.getAdditional(3));

            // check for overlap with spheres already generated
            auto checkOverlap = [&spheres](const Sphere& sphere) {
                for (const Sphere& s : spheres) {
                    if (s.intersects(sphere)) {
                        return true;
                    }
                }
                return false;
            };
            Sphere sphere(center, radius);
            if (checkOverlap(sphere)) {
                // overlaps, reject
                bailoutCounter++;
                continue;
            }

            // okay, this sphere seems valid, accept it
            break;
        }

        Sphere sphere(center, radius);

        // extract all particles inside the sphere, ignore particles outside of the domain
        Array<Vector> spherePositions;
        for (Size i = 0; i < positions.size();) {
            if (sphere.contains(positions[i])) {
                spherePositions.push(positions[i]);
                positions.remove(i);
            } else {
                ++i;
            }
        }

        // if the body has less than the minimal number of particles, reject it and generate a new sphere
        if (spherePositions.size() < minN) {
            // we need to put the (unused) points back
            positions.pushAll(std::move(spherePositions));
            bailoutCounter++;
            continue;
        }
        spheres.push(sphere);

        // create the body
        Storage body(material);
        body.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(spherePositions));
        this->setQuantities(body, *material, sphere.volume());

        // add it to the storage
        storage.merge(std::move(body));
        bodyIndex++;

        // we are still adding spheres, reset the counter
        bailoutCounter = 0;
    }

    SPH_ASSERT(!spheres.empty());
}

/// \brief Creates array of particles masses, assuming relation m ~ h^3.
///
/// This is equal to totalM / r.size() if all particles in the body have the same smoothing length.
static Array<Float> getMasses(ArrayView<const Vector> r, const Float totalM) {
    Array<Float> m(r.size());
    Float prelimM = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        m[i] = pow<3>(r[i][H]);
        prelimM += m[i];
    }
    // renormalize masses so that they sum up to totalM
    const Float normalization = totalM / prelimM;
    for (Size i = 0; i < r.size(); ++i) {
        m[i] *= normalization;
    }
    return m;
}

void InitialConditions::setQuantities(Storage& storage, IMaterial& material, const Float volume) {
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    const Float eta = material.getParam<Float>(BodySettingsId::SMOOTHING_LENGTH_ETA);
    for (Size i = 0; i < r.size(); ++i) {
        r[i][H] *= eta;
    }

    const Float rho0 = material.getParam<Float>(BodySettingsId::DENSITY);
    const Float totalM = volume * rho0; // m = rho * V
    SPH_ASSERT(totalM > 0._f);

    // Add masses (possibly heterogeneous, depending on generated smoothing lengths)
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, getMasses(r, totalM));

    // Mark particles of this body
    storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, bodyIndex);

    // Initialize material (we need density and energy for EoS)
    material.create(storage, context);

    // Generate mapping coordinates for textures
    if (context.uvMap) {
        Array<Vector> uvws = context.uvMap->generate(storage);
        storage.insert<Vector>(QuantityId::UVW, OrderEnum::ZERO, std::move(uvws));
    }
}

void repelParticles(ArrayView<Vector> r, const Float radius) {
    AutoPtr<ISymmetricFinder> finder = Factory::getFinder(RunSettings::getDefaults());
    finder->build(SEQUENTIAL, r);
    Array<NeighborRecord> neighs;
    Size moveCnt = -1;
    while (moveCnt != 0) {
        moveCnt = 0;
        for (Size i = 0; i < r.size(); ++i) {
            finder->findAll(i, 10._f * r[i][H] * radius, neighs);
            Vector force = Vector(0._f);
            if (neighs.size() <= 1) {
                continue;
            }
            for (NeighborRecord& n : neighs) {
                if (i != n.index) {
                    const Vector dr = r[n.index] - r[i];
                    force += -0.3_f * dr * pow<3>(r[i][H]) / pow<3>(getLength(dr));
                    if (getLength(dr) < r[i][H] * radius) {
                        moveCnt++;
                    }
                }
            }
            force[H] = 0._f;
            r[i] += force;
        }
    }
}

NAMESPACE_SPH_END
