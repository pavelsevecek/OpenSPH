#include "run/RubblePile.h"
#include "gravity/NBodySolver.h"
#include "io/LogFile.h"
#include "io/Logger.h"
#include "math/rng/VectorRng.h"
#include "objects/geometry/Domain.h"
#include "objects/geometry/Sphere.h"
#include "post/Analysis.h"
#include "run/Collision.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Presets.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

RubblePileRunPhase::RubblePileRunPhase(const Presets::CollisionParams params,
    SharedPtr<IRunCallbacks> callbacks)
    : collisionParams(params) {
    this->callbacks = callbacks;
    settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(0._f, 5.e4_f))
        .set(RunSettingsId::RUN_OUTPUT_INTERVAL, 1.e3_f)
        .set(RunSettingsId::SPH_KERNEL_ETA, 1.3_f)
        .set(RunSettingsId::COLLISION_HANDLER, CollisionHandlerEnum::ELASTIC_BOUNCE)
        .set(RunSettingsId::COLLISION_OVERLAP, OverlapEnum::REPEL)
        .set(RunSettingsId::COLLISION_RESTITUTION_NORMAL, 0.6_f)
        .set(RunSettingsId::COLLISION_RESTITUTION_TANGENT, 0.9_f)
        .set(RunSettingsId::COLLISION_ALLOWED_OVERLAP, 0.01_f)
        .set(RunSettingsId::GRAVITY_KERNEL, GravityKernelEnum::SOLID_SPHERES)
        .set(RunSettingsId::GRAVITY_OPENING_ANGLE, 0.8_f)
        .set(RunSettingsId::GRAVITY_LEAF_SIZE, 20)
        .set(RunSettingsId::GRAVITY_RECOMPUTATION_PERIOD, 10._f)
        .set(RunSettingsId::TIMESTEPPING_INTEGRATOR, TimesteppingEnum::LEAP_FROG)
        .set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-2_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_INCREASE, 0.01_f)
        .set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 10._f)
        .set(RunSettingsId::TIMESTEPPING_CRITERION, TimeStepCriterionEnum::ACCELERATION)
        .set(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR, 1._f);
}

static Vector sampleSphere(const Float radius, UniformRng& rng) {
    const Float l = rng(0);
    const Float u = rng(1) * 2._f - 1._f;
    const Float phi = rng(2) * 2._f * PI;

    const Float l13 = cbrt(l);
    const Float rho = radius * l13 * sqrt(1._f - sqr(u));
    const Float x = rho * cos(phi);
    const Float y = rho * sin(phi);
    const Float z = radius * l13 * u;

    return Vector(x, y, z);
}

void RubblePileRunPhase::setUp() {
    storage = makeShared<Storage>(makeShared<NullMaterial>(BodySettings::getDefaults()));
    solver = makeAuto<NBodySolver>(*scheduler, settings);
    logger = Factory::getLogger(settings);

    const Float radius = 1.4_f * collisionParams.targetRadius;
    const PowerLawSfd sfd{ 2._f, Interval(0.2_f * radius, 0.4_f * radius) };

    UniformRng rng;
    Array<Vector> positions;
    Size bailoutCounter = 0;
    const Float sep = 1._f;
    do {
        Vector v = sampleSphere(radius, rng);
        v[H] = sfd(rng(3));

        // check for intersections
        bool intersection = false;
        for (Size i = 0; i < positions.size(); ++i) {
            if (Sphere(v, sep * v[H]).intersects(Sphere(positions[i], sep * positions[i][H]))) {
                intersection = true;
                break;
            }
        }
        if (intersection) {
            // discard
            bailoutCounter++;
            continue;
        }
        positions.push(v);
        bailoutCounter = 0;
        logger->write("Generated sphere #", positions.size());

    } while (bailoutCounter < 1000);
    logger->write("Generating finished");

    // assign masses
    const Float rho = collisionParams.body.get<Float>(BodySettingsId::DENSITY);
    Array<Float> masses(positions.size());
    for (Size i = 0; i < positions.size(); ++i) {
        masses[i] = rho * sphereVolume(positions[i][H]);
    }

    storage->insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(positions));
    storage->insert<Float>(QuantityId::MASS, OrderEnum::ZERO, std::move(masses));

    solver->create(*storage, storage->getMaterial(0));

    triggers.pushBack(makeAuto<CommonStatsLog>(Factory::getLogger(settings), settings));
}

/// \brief Helper domain defined by a set of spheres
class SpheresDomain : public IDomain {
private:
    Array<Sphere> spheres;
    Sphere boundingSphere;
    Float bulkPorosity;
    mutable UniformRng rng;

public:
    explicit SpheresDomain(ArrayView<const Vector> r, const Float radius, const Float bulkPorosity)
        : bulkPorosity(bulkPorosity) {
        spheres.resize(r.size());
        for (Size i = 0; i < r.size(); ++i) {
            spheres[i] = Sphere(r[i], r[i][H]);
        }
        boundingSphere = Sphere(Vector(0._f), radius);
    }

    virtual Vector getCenter() const override {
        return Vector(0._f);
    }

    virtual Box getBoundingBox() const override {
        return boundingSphere.getBBox();
    }

    virtual Float getVolume() const override {
        Float volume = 0._f;
        for (const Sphere& s : spheres) {
            if (!s.intersects(boundingSphere)) {
                volume += s.volume();
            } else {
                // http://mathworld.wolfram.com/Sphere-SphereIntersection.html
                const Float R = boundingSphere.radius();
                const Float r = s.radius();
                const Float d = getLength(s.center() - boundingSphere.center());
                volume +=
                    PI * sqr(R + r - d) *
                    (sqr(d) + 2._f * d * r - 3._f * sqr(r) + 2._f * d * R + 6._f * r * R - 3._f * sqr(R)) /
                    (12._f * d);
            }
        }
        return /*bulkPorosity * */ volume;
    }

    virtual bool contains(const Vector& v) const override {
        if (!boundingSphere.contains(v)) {
            return false;
        }
        for (const Sphere& s : spheres) {
            if (s.contains(v)) {
                /*const Float r = getLength(v - s.center()) / s.radius();
                const Float p = porosityFunction(r);
                ASSERT(p >= 0._f && p <= 2._f);
                // probably a mistake in Deller's paper, we place the particle if the random value is GREATER
                // than the local porosity function
                return rng() > p;*/
                return true;
            }
        }
        return false;
    }

    virtual void getSubset(ArrayView<const Vector>, Array<Size>&, const SubsetType) const override {
        NOT_IMPLEMENTED;
    }

    virtual void getDistanceToBoundary(ArrayView<const Vector>, Array<Float>&) const override {
        NOT_IMPLEMENTED;
    }

    virtual void project(ArrayView<Vector>, Optional<ArrayView<Size>>) const override {
        NOT_IMPLEMENTED;
    }

    virtual void addGhosts(ArrayView<const Vector>, Array<Ghost>&, const Float, const Float) const override {
        NOT_IMPLEMENTED;
    }

private:
    /// \brief Porosity function introduced by Deller et al. 2016 (see Eq. 3 therein).
    Float porosityFunction(const Float r) const {
        ASSERT(r >= 0._f && r <= 1._f, r);
        return 2._f * sqr(r) / (2._f * sqr(r) - 2._f * r + 1._f) * bulkPorosity;
    }
};

void RubblePileRunPhase::tearDown(const Statistics& UNUSED(stats)) {
    // convert spheres to SPH particles (in place)
    AutoPtr<IDistribution> distribution = Factory::getDistribution(collisionParams.body);
    const Float bulkPorosity = collisionParams.body.get<Float>(BodySettingsId::BULK_POROSITY);
    SpheresDomain domain(
        storage->getValue<Vector>(QuantityId::POSITION), collisionParams.targetRadius, bulkPorosity);
    // SphericalDomain domain(Vector(0._f), params.targetRadius);

    // this domain is currently not thread-safe, so we need to generate particles sequentially
    Array<Vector> positions = distribution->generate(SEQUENTIAL, collisionParams.targetParticleCnt, domain);
    /// \todo multiply this automatically? (it is done in InitialConditions, but not elsewhere)
    const Float eta = settings.get<Float>(RunSettingsId::SPH_KERNEL_ETA);
    for (Size i = 0; i < positions.size(); ++i) {
        positions[i][H] *= eta;
    }

    Storage sph(Factory::getMaterial(collisionParams.body));
    sph.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(positions));

    const Float density = collisionParams.body.get<Float>(BodySettingsId::DENSITY);
    sph.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, density * domain.getVolume() / sph.getParticleCnt());
    sph.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, 0);

    // remove particles with too few neighbours
    /*Array<Size> neighCnts = Post::findNeighbourCounts(sph, 2._f);
    Array<Size> toRemove;
    for (Size i = 0; i < neighCnts.size(); ++i) {
        if (neighCnts[i] < 5) {
            toRemove.push(i);
        }
    }
    sph.remove(toRemove, Storage::RemoveFlag::INDICES_SORTED);*/

    *storage = std::move(sph);
}

void RubblePileRunPhase::handoff(Storage&& UNUSED(input)) {
    STOP;
}

AutoPtr<IRunPhase> RubblePileRunPhase::getNextPhase() const {
    TODO("Properly add into the pipeline");
    return makeAuto<StabilizationRunPhase>(collisionParams, PhaseParams{});
}

NAMESPACE_SPH_END
