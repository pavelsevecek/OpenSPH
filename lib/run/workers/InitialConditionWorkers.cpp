#include "run/workers/InitialConditionWorkers.h"
#include "objects/geometry/Sphere.h"
#include "physics/Eos.h"
#include "physics/Functions.h"
#include "post/Analysis.h"
#include "quantities/Quantity.h"
#include "run/IRun.h"
#include "run/SpecialEntries.h"
#include "sph/Materials.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "system/Settings.impl.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

// ----------------------------------------------------------------------------------------------------------
// MonolithicBodyIc
// ----------------------------------------------------------------------------------------------------------

MonolithicBodyIc::MonolithicBodyIc(const std::string& name, const BodySettings& overrides)
    : IParticleWorker(name)
    , MaterialProvider(overrides) {
    body.set(BodySettingsId::SMOOTHING_LENGTH_ETA, 1.3_f).set(BodySettingsId::DISTRIBUTE_MODE_SPH5, false);
}

UnorderedMap<std::string, WorkerType> MonolithicBodyIc::requires() const {
    UnorderedMap<std::string, WorkerType> map;
    if (slotUsage.shape) {
        map.insert("shape", WorkerType::GEOMETRY);
    }
    if (slotUsage.material) {
        map.insert("material", WorkerType::MATERIAL);
    }

    return map;
}

void MonolithicBodyIc::addParticleCategory(VirtualSettings& settings) {
    VirtualSettings::Category& particleCat = settings.addCategory("Particles");
    particleCat.connect<int>("Particle count", body, BodySettingsId::PARTICLE_COUNT)
        .connect<EnumWrapper>("Distribution", body, BodySettingsId::INITIAL_DISTRIBUTION)
        .connect<Float>("Radius multiplier", body, BodySettingsId::SMOOTHING_LENGTH_ETA)
        .connect<bool>("Exact distance", body, BodySettingsId::DISTRIBUTE_MODE_SPH5);
}

VirtualSettings MonolithicBodyIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    this->addParticleCategory(connector);

    VirtualSettings::Category& shapeCat = connector.addCategory("Shape");
    shapeCat
        .connect<bool>("Custom shape",
            "useShapeSlot",
            slotUsage.shape,
            "If true, a user-specified geometry input is used instead of shape parameters of the node.")
        .connect<EnumWrapper>(
            "Shape type", body, BodySettingsId::BODY_SHAPE_TYPE, [this] { return !slotUsage.shape; })
        .connect<Float>(
            "Radius [km]",
            body,
            BodySettingsId::BODY_RADIUS,
            [this] {
                const DomainEnum domain = body.get<DomainEnum>(BodySettingsId::BODY_SHAPE_TYPE);
                return !slotUsage.shape &&
                       (domain == DomainEnum::SPHERICAL || domain == DomainEnum::CYLINDER);
            },
            1.e3_f)
        .connect<Float>(
            "Height [km]",
            body,
            BodySettingsId::BODY_HEIGHT,
            [this] {
                const DomainEnum domain = body.get<DomainEnum>(BodySettingsId::BODY_SHAPE_TYPE);
                return !slotUsage.shape && domain == DomainEnum::CYLINDER;
            },
            1.e3_f)
        .connect<Vector>(
            "Dimensions [km]",
            body,
            BodySettingsId::BODY_DIMENSIONS,
            [this] {
                const DomainEnum domain = body.get<DomainEnum>(BodySettingsId::BODY_SHAPE_TYPE);
                return !slotUsage.shape && (domain == DomainEnum::BLOCK || domain == DomainEnum::ELLIPSOIDAL);
            },
            1.e3_f);

    VirtualSettings::Category& materialCat = connector.addCategory("Material");
    materialCat.connect<bool>("Custom material",
        "useMaterialSlot",
        slotUsage.material,
        "If true, a user-specified material input is used instead of material parameters of the node.");
    this->addMaterialEntries(materialCat, [this] { return !slotUsage.material; });

    auto diehlEnabler = [this] {
        return body.get<DistributionEnum>(BodySettingsId::INITIAL_DISTRIBUTION) ==
               DistributionEnum::DIEHL_ET_AL;
    };
    VirtualSettings::Category& diehlCat = connector.addCategory("Diehl's distribution");
    diehlCat.connect<int>("Iteration count", body, BodySettingsId::DIEHL_ITERATION_COUNT, diehlEnabler);
    diehlCat.connect<Float>("Strength", body, BodySettingsId::DIEHL_STRENGTH, diehlEnabler);

    VirtualSettings::Category& dynamicsCat = connector.addCategory("Dynamics");
    dynamicsCat.connect<Float>("Spin rate [rev/day]", body, BodySettingsId::BODY_SPIN_RATE);

    return connector;
}

class IcProgressCallback {
private:
    IRunCallbacks& callbacks;

public:
    IcProgressCallback(IRunCallbacks& callbacks)
        : callbacks(callbacks) {}

    bool operator()(const Float progress) const {
        Statistics stats;
        stats.set(StatisticsId::RELATIVE_PROGRESS, progress);
        callbacks.onTimeStep(Storage(), stats);
        return false;
    }
};

void MonolithicBodyIc::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    SharedPtr<IDomain> domain = slotUsage.shape ? this->getInput<IDomain>("shape") : Factory::getDomain(body);
    SharedPtr<IMaterial> material =
        slotUsage.material ? this->getInput<IMaterial>("material") : Factory::getMaterial(body);

    const DistributionEnum distType = body.get<DistributionEnum>(BodySettingsId::INITIAL_DISTRIBUTION);
    AutoPtr<IDistribution> distribution;
    if (distType == DistributionEnum::DIEHL_ET_AL) {
        DiehlParams diehl;
        diehl.numOfIters = body.get<int>(BodySettingsId::DIEHL_ITERATION_COUNT);
        diehl.strength = body.get<Float>(BodySettingsId::DIEHL_STRENGTH);
        diehl.onIteration = [&callbacks, iterCnt = diehl.numOfIters](
                                const Size i, const ArrayView<const Vector> positions) {
            Storage storage;
            Array<Vector> r;
            r.pushAll(positions.begin(), positions.end());
            storage.insert<Vector>(QuantityId::POSITION, OrderEnum::FIRST, std::move(r));
            Statistics stats;
            stats.set(StatisticsId::INDEX, int(i));
            stats.set(StatisticsId::RELATIVE_PROGRESS, Float(i) / iterCnt);

            if (i == 0) {
                callbacks.onSetUp(storage, stats);
            }
            callbacks.onTimeStep(storage, stats);
            return !callbacks.shouldAbortRun();
        };

        distribution = makeAuto<DiehlDistribution>(diehl);
    } else {
        distribution = Factory::getDistribution(body, IcProgressCallback(callbacks));
    }

    /// \todo less retarded way -- particle count has no place in material settings
    material->setParam(BodySettingsId::PARTICLE_COUNT, body.get<int>(BodySettingsId::PARTICLE_COUNT));
    material->setParam(
        BodySettingsId::SMOOTHING_LENGTH_ETA, body.get<Float>(BodySettingsId::SMOOTHING_LENGTH_ETA));

    // use defaults where no global parameters are provided
    RunSettings settings;
    settings.addEntries(global);
    InitialConditions ic(settings);

    result = makeShared<ParticleData>();

    BodyView view = ic.addMonolithicBody(result->storage, *domain, material, *distribution);
    const Float spinRate = body.get<Float>(BodySettingsId::BODY_SPIN_RATE) * 2._f * PI / (3600._f * 24._f);

    view.addRotation(Vector(0._f, 0._f, spinRate), domain->getCenter());
}

static WorkerRegistrar sRegisterMonolithic("create monolithic body",
    "body",
    "initial conditions",
    [](const std::string& name) { return makeAuto<MonolithicBodyIc>(name); });

// ----------------------------------------------------------------------------------------------------------
// DifferentiatedBodyIc
// ----------------------------------------------------------------------------------------------------------


DifferentiatedBodyIc::DifferentiatedBodyIc(const std::string& name)
    : IParticleWorker(name) {}

VirtualSettings DifferentiatedBodyIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& layersCat = connector.addCategory("Layers");
    layersCat.connect("Layer count", "layer_cnt", layerCnt);
    VirtualSettings::Category& particleCat = connector.addCategory("Particles");
    particleCat.connect<int>("Particle count", mainBody, BodySettingsId::PARTICLE_COUNT)
        .connect<EnumWrapper>("Distribution", mainBody, BodySettingsId::INITIAL_DISTRIBUTION);

    return connector;
}

void DifferentiatedBodyIc::evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) {
    InitialConditions::BodySetup mantle;
    mantle.domain = this->getInput<IDomain>("base shape");
    mantle.material = this->getInput<IMaterial>("base material");
    mantle.material->setParam(
        BodySettingsId::PARTICLE_COUNT, mainBody.get<int>(BodySettingsId::PARTICLE_COUNT));
    mantle.material->setParam(BodySettingsId::INITIAL_DISTRIBUTION,
        mainBody.get<DistributionEnum>(BodySettingsId::INITIAL_DISTRIBUTION));

    Array<InitialConditions::BodySetup> layers;
    for (int i = layerCnt - 1; i >= 0; --i) {
        InitialConditions::BodySetup& layer = layers.emplaceBack();
        layer.domain = this->getInput<IDomain>("shape " + std::to_string(i + 1));
        layer.material = this->getInput<IMaterial>("material " + std::to_string(i + 1));
    }

    result = makeShared<ParticleData>();
    InitialConditions ic(global);
    ic.addHeterogeneousBody(result->storage, mantle, std::move(layers));
}

static WorkerRegistrar sRegisterDifferentiated("create differentiated body",
    "body",
    "initial conditions",
    [](const std::string& name) { return makeAuto<DifferentiatedBodyIc>(name); });

// ----------------------------------------------------------------------------------------------------------
// ImpactorIc
// ----------------------------------------------------------------------------------------------------------

void ImpactorIc::addParticleCategory(VirtualSettings& settings) {
    VirtualSettings::Category& particleCat = settings.addCategory("Particles");
    particleCat.connect<EnumWrapper>("Distribution", body, BodySettingsId::INITIAL_DISTRIBUTION)
        .connect<Float>("Radius multiplier", body, BodySettingsId::SMOOTHING_LENGTH_ETA)
        .connect<bool>("Exact distance", body, BodySettingsId::DISTRIBUTE_MODE_SPH5);
}

static Float getTargetDensity(const Storage& storage) {
    ArrayView<const Float> m, rho;
    tie(m, rho) = storage.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY);

    Float volume = 0._f;
    for (Size i = 0; i < m.size(); ++i) {
        volume += m[i] / rho[i];
    }

    ASSERT(volume > 0._f, volume);
    return m.size() / volume;
}

void ImpactorIc::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    SharedPtr<IDomain> domain = slotUsage.shape ? this->getInput<IDomain>("shape") : Factory::getDomain(body);
    SharedPtr<ParticleData> target = this->getInput<ParticleData>("target");

    const Size minParticleCnt = body.get<int>(BodySettingsId::MIN_PARTICLE_COUNT);
    const Size particleCnt = getTargetDensity(target->storage) * domain->getVolume();
    body.set(BodySettingsId::PARTICLE_COUNT, max<int>(particleCnt, minParticleCnt));

    MonolithicBodyIc::evaluate(global, callbacks);
}

static WorkerRegistrar sRegisterImpactorBody("create impactor",
    "impactor",
    "initial conditions",
    [](const std::string& name) { return makeAuto<ImpactorIc>(name); });

// ----------------------------------------------------------------------------------------------------------
// EquilibriumIc
// ----------------------------------------------------------------------------------------------------------

VirtualSettings EquilibriumIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    return connector;
}

using MassShell = Tuple<Size, Float, Float>;

/// Returns array of "shells", sorted by radius, containing particle index, shell radius and integrated mass.
static Array<MassShell> getMassInRadius(const Storage& storage, const Vector& r0) {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);

    Array<MassShell> table(r.size());
    for (Size i = 0; i < r.size(); ++i) {
        table[i] = makeTuple(i, getLength(r[i] - r0), m[i]);
    }

    // sort by radius
    std::sort(table.begin(), table.end(), [](const MassShell& s1, const MassShell& s2) {
        return s1.get<1>() < s2.get<1>();
    });

    // integrate masses
    for (Size i = 1; i < r.size(); ++i) {
        table[i].get<2>() += table[i - 1].get<2>();
    }

    return table;
}

static Array<Float> integratePressure(const Storage& storage, const Vector& r0) {
    Array<MassShell> massInRadius = getMassInRadius(storage, r0);
    ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    Array<Float> p(massInRadius.size());
    Float p0 = 0._f; // ad hoc, will be corrected afterwards
    p.fill(p0);
    for (Size k = 1; k < massInRadius.size(); ++k) {
        const Size i = massInRadius[k].get<0>();
        const Float r = massInRadius[k].get<1>();
        const Float dr = r - massInRadius[k - 1].get<1>();
        ASSERT(dr >= 0._f);
        const Float M = massInRadius[k].get<2>();

        p[i] = p0 - Constants::gravity * M * rho[i] / sqr(r) * dr;
        p0 = p[i];
    }

    // subtract the surface pressure, so that the surface pressure is 0
    for (Size i = 0; i < p.size(); ++i) {
        p[i] -= p0;
    }

    return p;
}

void EquilibriumIc::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = this->getInput<ParticleData>("particles");
    Storage& storage = result->storage;

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    const Vector r0 = Post::getCenterOfMass(m, r);
    Sphere boundingSphere(r0, 0._f);
    for (Size i = 0; i < r.size(); ++i) {
        boundingSphere = Sphere(r0, max(boundingSphere.radius(), getLength(r[i] - r0)));
    }

    ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    ArrayView<Float> u = storage.getValue<Float>(QuantityId::ENERGY);
    Array<Float> p = integratePressure(storage, r0);

    for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
        MaterialView mat = storage.getMaterial(matId);
        RawPtr<EosMaterial> eosMat = dynamicCast<EosMaterial>(addressOf(mat.material()));
        ASSERT(eosMat);
        for (Size i : mat.sequence()) {
            u[i] = eosMat->getEos().getInternalEnergy(rho[i], p[i]);
        }
    }
}

static WorkerRegistrar sRegisterEquilibriumIc("set equilibrium energy",
    "equilibrium",
    "initial conditions",
    [](const std::string& name) { return makeAuto<EquilibriumIc>(name); });

// ----------------------------------------------------------------------------------------------------------
// ModifyQuantityIc
// ----------------------------------------------------------------------------------------------------------

enum class ChangeMode {
    PARAMETRIC,
    CURVE,
};

static RegisterEnum<ChangeMode> sChangeMode({
    { ChangeMode::PARAMETRIC, "parametric", "Specified by parameters" },
    { ChangeMode::CURVE, "curve", "Curve" },
});

enum class ChangeableQuantityId {
    DENSITY,
    ENERGY,
};

static RegisterEnum<ChangeableQuantityId> sSubsetType({
    { ChangeableQuantityId::DENSITY, "density", "Material density." },
    { ChangeableQuantityId::ENERGY, "specific energy", "Initial specific energy." },
});

ModifyQuantityIc::ModifyQuantityIc(const std::string& name)
    : IParticleWorker(name)
    , mode(ChangeMode::PARAMETRIC)
    , curve(makeAuto<CurveEntry>()) {
    id = EnumWrapper(ChangeableQuantityId::DENSITY);
}

VirtualSettings ModifyQuantityIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    auto paramEnabler = [this] { return ChangeMode(mode) == ChangeMode::PARAMETRIC; };
    auto curveEnabler = [this] { return ChangeMode(mode) == ChangeMode::CURVE; };

    VirtualSettings::Category& quantityCat = connector.addCategory("Modification");
    quantityCat.connect("Quantity", "quantity", id)
        .connect("Mode", "mode", mode)
        .connect("Center value [si]", "central_value", centralValue, paramEnabler)
        .connect("Radial gradient [si/km]", "radial_grad", radialGrad, 1.e-3f, paramEnabler)
        .connect("Curve", "curve", curve, curveEnabler);

    return connector;
}

void ModifyQuantityIc::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = this->getInput<ParticleData>("particles");

    ArrayView<const Vector> r = result->storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Float> q;
    switch (ChangeableQuantityId(id)) {
    case ChangeableQuantityId::DENSITY:
        q = result->storage.getValue<Float>(QuantityId::DENSITY);
        break;
    case ChangeableQuantityId::ENERGY:
        q = result->storage.getValue<Float>(QuantityId::ENERGY);
        break;
    default:
        NOT_IMPLEMENTED;
    }

    switch (ChangeMode(mode)) {
    case ChangeMode::PARAMETRIC:
        for (Size i = 0; i < r.size(); ++i) {
            const Float dist = getLength(r[i]);
            q[i] = centralValue + radialGrad * dist;
        }
        break;
    case ChangeMode::CURVE: {
        RawPtr<CurveEntry> curveEntry = dynamicCast<CurveEntry>(curve.getEntry());
        const Curve func = curveEntry->getCurve();
        for (Size i = 0; i < r.size(); ++i) {
            const Float dist = getLength(r[i]);
            q[i] = func(dist);
        }
        break;
    }
    default:
        NOT_IMPLEMENTED;
    }
}

static WorkerRegistrar sRegisterModifyQuantityIc("modify quantity",
    "modifier",
    "initial conditions",
    [](const std::string& name) { return makeAuto<ModifyQuantityIc>(name); });

// ----------------------------------------------------------------------------------------------------------
// NBodyIc
// ----------------------------------------------------------------------------------------------------------

// clang-format off
template <>
AutoPtr<NBodySettings> NBodySettings::instance(new NBodySettings{
    { NBodySettingsId::PARTICLE_COUNT,      "particles.count",        10000,
        "Number of generated particles." },
    { NBodySettingsId::TOTAL_MASS,          "total_mass",             Constants::M_earth,
        "Total mass of the particles. Masses of individual particles depend on total number "
        "of particles and on particle sizes." },
    { NBodySettingsId::DOMAIN_RADIUS,       "domain.radius",          100.e3_f,
        "Radius of the domain where the particles are initially generated. This is not a boundary, particles "
        "can leave the domain. " },
    { NBodySettingsId::RADIAL_PROFILE,      "radial_profile",         1.5_f,
        "Specifies a balance between particle concentration in the center of the domain and at the boundary. "
        "Higher values imply more dense center and fewer particles at the boundary." },
    { NBodySettingsId::HEIGHT_SCALE,        "height_scale",           1._f,
        "Specifies the relative scale of the domain in z-direction. For 1, the domain is spherical, lower values "
        "can be used to create a disk-like domain." },
    { NBodySettingsId::POWER_LAW_INTERVAL,  "power_law.interval",     Interval(1.e3_f, 10.e3_f),
        "Interval of sizes generated particles." },
    { NBodySettingsId::POWER_LAW_EXPONENT,  "power_law.exponent",     2._f,
        "Exponent of the power-law, used to generate particle sizes." },
    { NBodySettingsId::VELOCITY_MULTIPLIER, "velocity.multiplier",    1._f,
        "Multiplier of the Keplerian velocity of particles. " },
    { NBodySettingsId::VELOCITY_DISPERSION, "velocity.dispersion",    10._f,
        "Specifies a random component of initial particle velocities." },

});
// clang-format on

template class Settings<NBodySettingsId>;

NBodyIc::NBodyIc(const std::string& name, const NBodySettings& overrides)
    : IParticleWorker(name) {
    settings.addEntries(overrides);
}

VirtualSettings NBodyIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& particleCat = connector.addCategory("Particles");
    particleCat.connect<int>("Particle count", settings, NBodySettingsId::PARTICLE_COUNT);

    VirtualSettings::Category& distributionCat = connector.addCategory("Distribution");
    distributionCat.connect<Float>("Domain radius [km]", settings, NBodySettingsId::DOMAIN_RADIUS, 1.e3_f)
        .connect<Float>("Radial exponent", settings, NBodySettingsId::RADIAL_PROFILE)
        .connect<Float>("Height scale", settings, NBodySettingsId::HEIGHT_SCALE);
    distributionCat.addEntry("min_size",
        makeEntry(settings, NBodySettingsId::POWER_LAW_INTERVAL, "Minimal size [m]", IntervalBound::LOWER));
    distributionCat.addEntry("max_size",
        makeEntry(settings, NBodySettingsId::POWER_LAW_INTERVAL, "Maximal size [m]", IntervalBound::UPPER));

    distributionCat.connect<Float>("Power-law exponent", settings, NBodySettingsId::POWER_LAW_EXPONENT);

    VirtualSettings::Category& dynamicsCat = connector.addCategory("Dynamics");
    dynamicsCat
        .connect<Float>("Total mass [M_earth]", settings, NBodySettingsId::TOTAL_MASS, Constants::M_earth)
        .connect<Float>("Velocity multiplier", settings, NBodySettingsId::VELOCITY_MULTIPLIER)
        .connect<Float>("Velocity dispersion [km/s]", settings, NBodySettingsId::VELOCITY_DISPERSION, 1.e3_f);

    return connector;
}

static Vector sampleSphere(const Float radius, const Float exponent, IRng& rng) {
    const Float l = rng(0);
    const Float u = rng(1) * 2._f - 1._f;
    const Float phi = rng(2) * 2._f * PI;

    const Float l13 = pow(l, exponent); // cbrt(l);
    const Float rho = radius * l13 * sqrt(1._f - sqr(u));
    const Float x = rho * cos(phi);
    const Float y = rho * sin(phi);
    const Float z = radius * l13 * u;

    return Vector(x, y, z);
}

void NBodyIc::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    const Size particleCnt = settings.get<int>(NBodySettingsId::PARTICLE_COUNT);
    const Float radius = settings.get<Float>(NBodySettingsId::DOMAIN_RADIUS);
    const Float radialExponent = settings.get<Float>(NBodySettingsId::RADIAL_PROFILE);
    const Float heightScale = settings.get<Float>(NBodySettingsId::HEIGHT_SCALE);
    const Float velocityMult = settings.get<Float>(NBodySettingsId::VELOCITY_MULTIPLIER);
    const Float velocityDispersion = settings.get<Float>(NBodySettingsId::VELOCITY_DISPERSION);
    const Float totalMass = settings.get<Float>(NBodySettingsId::TOTAL_MASS);
    const Interval interval = settings.get<Interval>(NBodySettingsId::POWER_LAW_INTERVAL);
    const Float sizeExponent = settings.get<Float>(NBodySettingsId::POWER_LAW_EXPONENT);
    const PowerLawSfd sfd{ sizeExponent, interval };

    AutoPtr<IRng> rng = Factory::getRng(global);
    Array<Vector> positions;
    Size bailoutCounter = 0;
    const Float sep = 1._f;
    const Size reportStep = max(particleCnt / 1000, 1u);
    do {
        Vector v = sampleSphere(radius, radialExponent, *rng);
        v[Z] *= heightScale;
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

        if (positions.size() % reportStep == reportStep - 1) {
            Statistics stats;
            stats.set(StatisticsId::RELATIVE_PROGRESS, Float(positions.size()) / particleCnt);
            callbacks.onTimeStep(Storage(), stats);
        }

    } while (positions.size() < particleCnt && bailoutCounter < 1000);

    // assign masses
    Array<Float> masses(positions.size());

    Float m_sum = 0._f;
    for (Size i = 0; i < positions.size(); ++i) {
        masses[i] = sphereVolume(positions[i][H]);
        m_sum += masses[i];
    }

    // assign velocities
    Array<Vector> velocities(positions.size());
    for (Size i = 0; i < positions.size(); ++i) {
        masses[i] *= totalMass / m_sum;
        ASSERT(masses[i] > 0._f);

        const Float r0 = getLength(positions[i]);
        const Float m0 = totalMass * sphereVolume(r0) / sphereVolume(radius);
        const Float v_kepl = velocityMult * sqrt(Constants::gravity * m0 / r0);
        const Vector dir = getNormalized(Vector(positions[i][Y], -positions[i][X], 0._f));
        Vector v_random = sampleSphere(velocityDispersion, 0.333_f, *rng);
        v_random[Z] *= heightScale;
        velocities[i] = dir * v_kepl + v_random;
    }

    Storage storage(makeAuto<NullMaterial>(BodySettings::getDefaults()));
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(positions));
    storage.getDt<Vector>(QuantityId::POSITION) = std::move(velocities);
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, std::move(masses));
    storage.insert<Vector>(QuantityId::ANGULAR_FREQUENCY, OrderEnum::ZERO, Vector(0._f));

    result = makeShared<ParticleData>();
    result->storage = std::move(storage);
}

static WorkerRegistrar sRegisterNBodyIc("N-body ICs", "initial conditions", [](const std::string& name) {
    return makeAuto<NBodyIc>(name);
});


// ----------------------------------------------------------------------------------------------------------
// GalaxyICs
// ----------------------------------------------------------------------------------------------------------

extern template class Settings<GalaxySettingsId>;

GalaxyIc::GalaxyIc(const std::string& name, const GalaxySettings& overrides)
    : IParticleWorker(name) {
    settings.addEntries(overrides);
}

VirtualSettings GalaxyIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& diskCat = connector.addCategory("Disk");
    diskCat.connect<int>("Disk particle count", settings, GalaxySettingsId::DISK_PARTICLE_COUNT)
        .connect<Float>("Disk radial scale", settings, GalaxySettingsId::DISK_RADIAL_SCALE)
        .connect<Float>("Disk radial cutoff", settings, GalaxySettingsId::DISK_RADIAL_CUTOFF)
        .connect<Float>("Disk vertical scale", settings, GalaxySettingsId::DISK_VERTICAL_SCALE)
        .connect<Float>("Disk vertical cutoff", settings, GalaxySettingsId::DISK_VERTICAL_CUTOFF)
        .connect<Float>("Disk mass", settings, GalaxySettingsId::DISK_MASS)
        .connect<Float>("Toomre Q parameter", settings, GalaxySettingsId::DISK_TOOMRE_Q);

    VirtualSettings::Category& haloCat = connector.addCategory("Halo");
    haloCat.connect<int>("Halo particle count", settings, GalaxySettingsId::HALO_PARTICLE_COUNT)
        .connect<Float>("Halo scale length", settings, GalaxySettingsId::HALO_SCALE_LENGTH)
        .connect<Float>("Halo cutoff", settings, GalaxySettingsId::HALO_CUTOFF)
        .connect<Float>("Halo gamma", settings, GalaxySettingsId::HALO_GAMMA)
        .connect<Float>("Halo mass", settings, GalaxySettingsId::HALO_MASS);

    VirtualSettings::Category& bulgeCat = connector.addCategory("Bulge");
    bulgeCat.connect<int>("Bulge particle count", settings, GalaxySettingsId::BULGE_PARTICLE_COUNT)
        .connect<Float>("Bulge scale length", settings, GalaxySettingsId::BULGE_SCALE_LENGTH)
        .connect<Float>("Bulge cutoff", settings, GalaxySettingsId::BULGE_CUTOFF)
        .connect<Float>("Bulge mass", settings, GalaxySettingsId::BULGE_MASS);

    VirtualSettings::Category& particleCat = connector.addCategory("Particles");
    particleCat.connect<Float>("Particle radius", settings, GalaxySettingsId::PARTICLE_RADIUS);

    return connector;
}

void GalaxyIc::evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) {
    Storage storage = Galaxy::generateIc(global, settings);

    result = makeShared<ParticleData>();
    result->storage = std::move(storage);

    /// \todo generalize units
    result->overrides.set(RunSettingsId::GRAVITY_CONSTANT, 1._f);
}


static WorkerRegistrar sRegisterGalaxyIc("galaxy ICs", "initial conditions", [](const std::string& name) {
    return makeAuto<GalaxyIc>(name);
});

NAMESPACE_SPH_END
