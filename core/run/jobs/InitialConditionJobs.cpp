#include "run/jobs/InitialConditionJobs.h"
#include "gravity/IGravity.h"
#include "objects/finders/PointCloud.h"
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
#include "sph/initial/Stellar.h"
#include "sph/solvers/EquilibriumSolver.h"
#include "system/Factory.h"
#include "system/Settings.impl.h"
#include "system/Statistics.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

// ----------------------------------------------------------------------------------------------------------
// MonolithicBodyIc
// ----------------------------------------------------------------------------------------------------------

MonolithicBodyIc::MonolithicBodyIc(const std::string& name, const BodySettings& overrides)
    : IParticleJob(name)
    , MaterialProvider(overrides) {
    body.set(BodySettingsId::SMOOTHING_LENGTH_ETA, 1.3_f).set(BodySettingsId::DISTRIBUTE_MODE_SPH5, false);
}

UnorderedMap<std::string, ExtJobType> MonolithicBodyIc::requires() const {
    UnorderedMap<std::string, ExtJobType> map;
    if (slotUsage.shape) {
        map.insert("shape", JobType::GEOMETRY);
    }
    if (slotUsage.material) {
        map.insert("material", JobType::MATERIAL);
    }

    return map;
}

void MonolithicBodyIc::addParticleCategory(VirtualSettings& settings) {
    VirtualSettings::Category& particleCat = settings.addCategory("Particles");
    particleCat.connect<int>("Particle count", body, BodySettingsId::PARTICLE_COUNT);
    particleCat.connect<EnumWrapper>("Distribution", body, BodySettingsId::INITIAL_DISTRIBUTION);
    particleCat.connect<Float>("Radius multiplier", body, BodySettingsId::SMOOTHING_LENGTH_ETA);
    particleCat.connect<bool>("Exact distance", body, BodySettingsId::DISTRIBUTE_MODE_SPH5);
    particleCat.connect<bool>("Center particles", body, BodySettingsId::CENTER_PARTICLES);
}

VirtualSettings MonolithicBodyIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    this->addParticleCategory(connector);

    VirtualSettings::Category& shapeCat = connector.addCategory("Shape");
    shapeCat.connect<bool>("Custom shape", "useShapeSlot", slotUsage.shape)
        .setTooltip(
            "If true, a user-specified geometry input is used instead of shape parameters of the node.");
    shapeCat.connect<EnumWrapper>("Shape type", body, BodySettingsId::BODY_SHAPE_TYPE).setEnabler([this] {
        return !slotUsage.shape;
    });
    shapeCat.connect<Float>("Radius [km]", body, BodySettingsId::BODY_RADIUS)
        .setEnabler([this] {
            const DomainEnum domain = body.get<DomainEnum>(BodySettingsId::BODY_SHAPE_TYPE);
            return !slotUsage.shape && (domain == DomainEnum::SPHERICAL || domain == DomainEnum::CYLINDER);
        })
        .setUnits(1.e3_f);
    shapeCat.connect<Float>("Height [km]", body, BodySettingsId::BODY_HEIGHT)
        .setEnabler([this] {
            const DomainEnum domain = body.get<DomainEnum>(BodySettingsId::BODY_SHAPE_TYPE);
            return !slotUsage.shape && domain == DomainEnum::CYLINDER;
        })
        .setUnits(1.e3_f);
    shapeCat.connect<Vector>("Dimensions [km]", body, BodySettingsId::BODY_DIMENSIONS)
        .setEnabler([this] {
            const DomainEnum domain = body.get<DomainEnum>(BodySettingsId::BODY_SHAPE_TYPE);
            return !slotUsage.shape && (domain == DomainEnum::BLOCK || domain == DomainEnum::ELLIPSOIDAL);
        })
        .setUnits(1.e3_f);

    VirtualSettings::Category& materialCat = connector.addCategory("Material");
    materialCat.connect<bool>("Custom material", "useMaterialSlot", slotUsage.material)
        .setTooltip(
            "If true, a user-specified material input is used instead of material parameters of the node.");
    this->addMaterialEntries(materialCat, [this] { return !slotUsage.material; });

    auto diehlEnabler = [this] {
        return body.get<DistributionEnum>(BodySettingsId::INITIAL_DISTRIBUTION) ==
               DistributionEnum::DIEHL_ET_AL;
    };
    VirtualSettings::Category& diehlCat = connector.addCategory("Diehl's distribution");
    diehlCat.connect<int>("Iteration count", body, BodySettingsId::DIEHL_ITERATION_COUNT)
        .setEnabler(diehlEnabler);
    diehlCat.connect<Float>("Strength", body, BodySettingsId::DIEHL_STRENGTH).setEnabler(diehlEnabler);

    VirtualSettings::Category& dynamicsCat = connector.addCategory("Dynamics");
    dynamicsCat.connect<Float>("Spin rate [rev/day]", body, BodySettingsId::BODY_SPIN_RATE);

    VirtualSettings::Category& visCat = connector.addCategory("Visualization");
    visCat.connect<Path>("Texture path", body, BodySettingsId::VISUALIZATION_TEXTURE)
        .setPathType(IVirtualEntry::PathType::INPUT_FILE)
        .setFileFormats({
            { "JPEG image", "jpg" },
            { "PNG image", "png" },
            { "TIFF image", "tif" },
        });

    return connector;
}

class IcProgressCallback {
private:
    IRunCallbacks& callbacks;

public:
    explicit IcProgressCallback(IRunCallbacks& callbacks)
        : callbacks(callbacks) {}

    bool operator()(const Float progress) const {
        Statistics stats;
        stats.set(StatisticsId::RELATIVE_PROGRESS, progress);
        callbacks.onTimeStep(Storage(), stats);
        return false;
    }
};

class DiehlReporter {
    IRunCallbacks& callbacks;
    int iterCnt;

public:
    DiehlReporter(IRunCallbacks& callbacks, const int iterCnt)
        : callbacks(callbacks)
        , iterCnt(iterCnt) {}

    bool operator()(const Size i, const ArrayView<const Vector> positions) const {
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
    }
};

void MonolithicBodyIc::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    SharedPtr<IDomain> domain = slotUsage.shape ? this->getInput<IDomain>("shape") : Factory::getDomain(body);
    SharedPtr<IMaterial> material =
        slotUsage.material ? this->getInput<IMaterial>("material") : Factory::getMaterial(body);

    // override the material texture
    std::string texturePath = body.get<std::string>(BodySettingsId::VISUALIZATION_TEXTURE);
    material->setParam(BodySettingsId::VISUALIZATION_TEXTURE, std::move(texturePath));

    const DistributionEnum distType = body.get<DistributionEnum>(BodySettingsId::INITIAL_DISTRIBUTION);
    AutoPtr<IDistribution> distribution;
    if (distType == DistributionEnum::DIEHL_ET_AL) {
        DiehlParams diehl;
        diehl.numOfIters = body.get<int>(BodySettingsId::DIEHL_ITERATION_COUNT);
        diehl.strength = body.get<Float>(BodySettingsId::DIEHL_STRENGTH);
        diehl.onIteration = DiehlReporter(callbacks, diehl.numOfIters);

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

static JobRegistrar sRegisterMonolithic(
    "create monolithic body",
    "body",
    "initial conditions",
    [](const std::string& name) { return makeAuto<MonolithicBodyIc>(name); },
    "Creates a single monolithic homogeneous body.");

// ----------------------------------------------------------------------------------------------------------
// DifferentiatedBodyIc
// ----------------------------------------------------------------------------------------------------------

DifferentiatedBodyIc::DifferentiatedBodyIc(const std::string& name)
    : IParticleJob(name) {}

VirtualSettings DifferentiatedBodyIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& layersCat = connector.addCategory("Layers");
    layersCat.connect("Layer count", "layer_cnt", layerCnt);
    VirtualSettings::Category& particleCat = connector.addCategory("Particles");
    particleCat.connect<int>("Particle count", mainBody, BodySettingsId::PARTICLE_COUNT);
    particleCat.connect<Float>("Radius multiplier", mainBody, BodySettingsId::SMOOTHING_LENGTH_ETA);
    particleCat.connect<EnumWrapper>("Distribution", mainBody, BodySettingsId::INITIAL_DISTRIBUTION);

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
    const Float eta = mainBody.get<Float>(BodySettingsId::SMOOTHING_LENGTH_ETA);
    mantle.material->setParam(BodySettingsId::SMOOTHING_LENGTH_ETA, eta);

    Array<InitialConditions::BodySetup> layers;
    for (int i = layerCnt - 1; i >= 0; --i) {
        InitialConditions::BodySetup& layer = layers.emplaceBack();
        layer.domain = this->getInput<IDomain>("shape " + std::to_string(i + 1));
        layer.material = this->getInput<IMaterial>("material " + std::to_string(i + 1));
        layer.material->setParam(BodySettingsId::SMOOTHING_LENGTH_ETA, eta);
    }

    result = makeShared<ParticleData>();
    InitialConditions ic(global);
    ic.addHeterogeneousBody(result->storage, mantle, std::move(layers));
}

static JobRegistrar sRegisterDifferentiated(
    "create differentiated body",
    "body",
    "initial conditions",
    [](const std::string& name) { return makeAuto<DifferentiatedBodyIc>(name); },
    "Creates a body consisting of multiple different materials. The base shape/material describes the "
    "global shape of body and material of a particles not assigned to any layer. The indexed layers than "
    "assign a specific material to a subset of particles.");

// ----------------------------------------------------------------------------------------------------------
// SingleParticleIc
// ----------------------------------------------------------------------------------------------------------

VirtualSettings SingleParticleIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& particleCat = connector.addCategory("Particle");
    particleCat.connect("Mass [M_sun]", "mass", mass).setUnits(Constants::M_sun);
    particleCat.connect("Radius [R_sun]", "radius", radius).setUnits(Constants::R_sun);
    particleCat.connect("Position [R_sun]", "r0", r0).setUnits(Constants::R_sun);
    particleCat.connect("Velocity [R_sun/yr]", "v0", v0).setUnits(Constants::R_sun / Constants::year);
    particleCat.connect("Flag", "flag", flag);

    return connector;
}

void SingleParticleIc::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = makeShared<ParticleData>();
    BodySettings body;
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
        .set(BodySettingsId::EOS, EosEnum::IDEAL_GAS); /// \todo only to allow pressure, should be done better
    result->storage = Storage(Factory::getMaterial(body));

    Vector pos = r0;
    pos[H] = radius;
    v0[H] = 0._f;
    result->storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, Array<Vector>({ pos }));
    result->storage.getDt<Vector>(QuantityId::POSITION)[0] = v0;
    result->storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, mass);
    result->storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, flag);
}

static JobRegistrar sRegisterSingleParticle(
    "create single particle",
    "particle",
    "initial conditions",
    [](const std::string& name) { return makeAuto<SingleParticleIc>(name); },
    "Creates a single particle with given mass, providing a convenient central potential for simulations of "
    "circumplanetary (circumstelar, circumbinary) disk.");

// ----------------------------------------------------------------------------------------------------------
// ImpactorIc
// ----------------------------------------------------------------------------------------------------------

void ImpactorIc::addParticleCategory(VirtualSettings& settings) {
    VirtualSettings::Category& particleCat = settings.addCategory("Particles");
    particleCat.connect<int>("Min particle count", body, BodySettingsId::MIN_PARTICLE_COUNT);
    particleCat.connect<EnumWrapper>("Distribution", body, BodySettingsId::INITIAL_DISTRIBUTION);
    particleCat.connect<Float>("Radius multiplier", body, BodySettingsId::SMOOTHING_LENGTH_ETA);
    particleCat.connect<bool>("Exact distance", body, BodySettingsId::DISTRIBUTE_MODE_SPH5);
}

static Float getTargetDensity(const Storage& storage) {
    ArrayView<const Float> m, rho;
    tie(m, rho) = storage.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY);

    Float volume = 0._f;
    for (Size i = 0; i < m.size(); ++i) {
        volume += m[i] / rho[i];
    }

    SPH_ASSERT(volume > 0._f, volume);
    return m.size() / volume;
}

void ImpactorIc::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    SharedPtr<IDomain> domain = slotUsage.shape ? this->getInput<IDomain>("shape") : Factory::getDomain(body);
    SharedPtr<ParticleData> target = this->getInput<ParticleData>("target");

    const Size minParticleCnt = body.get<int>(BodySettingsId::MIN_PARTICLE_COUNT);
    const Size particleCnt = Size(getTargetDensity(target->storage) * domain->getVolume());
    body.set(BodySettingsId::PARTICLE_COUNT, max<int>(particleCnt, minParticleCnt));

    MonolithicBodyIc::evaluate(global, callbacks);
}

static JobRegistrar sRegisterImpactorBody(
    "create impactor",
    "impactor",
    "initial conditions",
    [](const std::string& name) { return makeAuto<ImpactorIc>(name); },
    "Creates a monolithic body with automatic particle count. The number of particles is assigned "
    "to match the particle concentration (number density) of a target body.");

// ----------------------------------------------------------------------------------------------------------
// EquilibriumIc
// ----------------------------------------------------------------------------------------------------------

enum class EquilSolveEnum {
    SPHERICAL,
    PRECISE,
};

static RegisterEnum<EquilSolveEnum> sSolverType({
    { EquilSolveEnum::SPHERICAL,
        "spherical",
        "Computes equilibrium assuming spherically symmetric configuration." },
    { EquilSolveEnum::PRECISE, "precise", "Computes equilibrium by solving a least-squares problem." },
});

EquilibriumIc::EquilibriumIc(const std::string& name)
    : IParticleJob(name)
    , solver(EquilSolveEnum::SPHERICAL) {
    boundaryThreshold = 40;
}

VirtualSettings EquilibriumIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& solverCat = connector.addCategory("Solution");
    solverCat.connect("Solver", "solver", solver);
    solverCat.connect("Boundary threshold", "threshold", boundaryThreshold).setEnabler([&] {
        return EquilSolveEnum(solver) == EquilSolveEnum::PRECISE;
    });

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
        SPH_ASSERT(dr >= 0._f);
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

static void solveSpherical(Storage& storage) {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    const Vector r0 = Post::getCenterOfMass(m, r);
    Sphere boundingSphere(r0, 0._f);
    for (Size i = 0; i < r.size(); ++i) {
        boundingSphere = Sphere(r0, max(boundingSphere.radius(), getLength(r[i] - r0)));
    }

    ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    ArrayView<Float> u = storage.getValue<Float>(QuantityId::ENERGY);
    ArrayView<Float> p = storage.getValue<Float>(QuantityId::PRESSURE);
    Array<Float> solution = integratePressure(storage, r0);

    for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
        MaterialView mat = storage.getMaterial(matId);
        RawPtr<EosMaterial> eosMat = dynamicCast<EosMaterial>(addressOf(mat.material()));
        SPH_ASSERT(eosMat);

        for (Size i : mat.sequence()) {
            p[i] = solution[i];
            u[i] = eosMat->getEos().getInternalEnergy(rho[i], p[i]);
        }
    }
}

void EquilibriumIc::evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) {
    result = this->getInput<ParticleData>("particles");
    Storage& storage = result->storage;

    switch (EquilSolveEnum(solver)) {
    case EquilSolveEnum::SPHERICAL:
        solveSpherical(storage);
        break;
    case EquilSolveEnum::PRECISE: {
#ifdef SPH_USE_EIGEN
        RunSettings settings;
        settings.addEntries(global);
        SharedPtr<IScheduler> scheduler = Factory::getScheduler(settings);
        AutoPtr<IGravity> gravity = Factory::getGravity(settings);
        EquilibriumEnergySolver solver(*scheduler, settings, std::move(gravity), boundaryThreshold);
        Statistics stats;
        Outcome result = solver.solve(storage, stats);
        if (!result) {
            throw InvalidSetup("Cannot find equilibrium solution: " + result.error());
        }
#else
        MARK_USED(global);
        MARK_USED(boundaryThreshold);
        throw InvalidSetup("OpenSPH needs to be compiled with CONFIG+=use_eigen to use this option");
#endif
        break;
    }
    default:
        throw InvalidSetup("Unknown equilibrium solver");
    }
}

static JobRegistrar sRegisterEquilibriumIc(
    "set equilibrium energy",
    "equilibrium",
    "initial conditions",
    [](const std::string& name) { return makeAuto<EquilibriumIc>(name); },
    "Modifies the internal energy of the input body to create a pressure gradient that balances "
    "the gravitational acceleration. This can be used only for material with equation of state, "
    "it further expects spherical symmetry of the input body (although homogeneity is not "
    "required).");

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

static RegisterEnum<ChangeableQuantityId> sChangeableQuantity({
    { ChangeableQuantityId::DENSITY, "density", "Material density." },
    { ChangeableQuantityId::ENERGY, "specific energy", "Initial specific energy." },
});

ModifyQuantityIc::ModifyQuantityIc(const std::string& name)
    : IParticleJob(name)
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
    quantityCat.connect("Quantity", "quantity", id);
    quantityCat.connect("Mode", "mode", mode);
    quantityCat.connect("Center value [si]", "central_value", centralValue).setEnabler(paramEnabler);
    quantityCat.connect("Radial gradient [si/km]", "radial_grad", radialGrad)
        .setUnits(1.e-3f)
        .setEnabler(paramEnabler);
    quantityCat.connect("Curve", "curve", curve).setEnabler(curveEnabler);

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

static JobRegistrar sRegisterModifyQuantityIc(
    "modify quantity",
    "modifier",
    "initial conditions",
    [](const std::string& name) { return makeAuto<ModifyQuantityIc>(name); },
    "Modifies given quantity of the input body, optionally specifying a radial gradient or generic radial "
    "dependency via a user-defined curve.");

// ----------------------------------------------------------------------------------------------------------
// NoiseQuantity
// ----------------------------------------------------------------------------------------------------------

enum class NoiseQuantityId {
    DENSITY,
    VELOCITY,
};

static RegisterEnum<NoiseQuantityId> sNoiseQuantity({
    { NoiseQuantityId::DENSITY, "density", "Material density" },
    { NoiseQuantityId::VELOCITY, "velocity", "Particle velocity" },
});

const Indices GRID_DIMS(8, 8, 8);

NoiseQuantityIc::NoiseQuantityIc(const std::string& name)
    : IParticleJob(name) {
    id = EnumWrapper(NoiseQuantityId::DENSITY);
}

VirtualSettings NoiseQuantityIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& quantityCat = connector.addCategory("Noise parameters");
    quantityCat.connect("Quantity", "quantity", id);
    quantityCat.connect("Mean [si]", "mean", mean);
    quantityCat.connect("Magnitude [si]", "magnitude", magnitude);

    return connector;
}

void NoiseQuantityIc::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& callbacks) {
    result = this->getInput<ParticleData>("particles");
    Storage& storage = result->storage;
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);

    switch (NoiseQuantityId(id)) {
    case NoiseQuantityId::DENSITY: {
        ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
        this->randomize<1>(callbacks, r, [&rho](Float value, Size i, Size UNUSED(j)) { //
            rho[i] = value;
        });
        break;
    }
    case NoiseQuantityId::VELOCITY: {
        ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        this->randomize<3>(callbacks, r, [&v](Float value, Size i, Size j) { v[i][j] = value; });
        break;
    }
    default:
        NOT_IMPLEMENTED;
    }
}

template <Size Dims, typename TSetter>
void NoiseQuantityIc::randomize(IRunCallbacks& callbacks,
    ArrayView<const Vector> r,
    const TSetter& setter) const {
    UniformRng rng;

    StaticArray<Grid<Vector>, Dims> gradients;
    for (Size dim = 0; dim < Dims; ++dim) {
        gradients[dim] = Grid<Vector>(GRID_DIMS);
        for (Vector& grad : gradients[dim]) {
            grad = sampleUnitSphere(rng);
        }
    }

    Box box;
    for (Size i = 0; i < r.size(); ++i) {
        box.extend(r[i] + Vector(r[i][H]));
        box.extend(r[i] - Vector(r[i][H]));
    }

    Statistics stats;
    for (Size i = 0; i < r.size(); ++i) {

        for (Size dim = 0; dim < Dims; ++dim) {
            const Vector pos = (r[i] - box.lower()) / box.size() * GRID_DIMS;
            const Float value = mean + magnitude * perlin(gradients[dim], pos);
            SPH_ASSERT(isReal(value));
            setter(value, i, dim);
        }

        if (i % 1000 == 0) {
            stats.set(StatisticsId::RELATIVE_PROGRESS, Float(i) / r.size());
            callbacks.onTimeStep({}, stats);
        }
    }
}

Float NoiseQuantityIc::perlin(const Grid<Vector>& gradients, const Vector& v) const {
    const Indices v0(v);
    const Vector dv = Vector(v) - v0;

    const Float x000 = this->dotGradient(gradients, v0 + Indices(0, 0, 0), v);
    const Float x001 = this->dotGradient(gradients, v0 + Indices(0, 0, 1), v);
    const Float x010 = this->dotGradient(gradients, v0 + Indices(0, 1, 0), v);
    const Float x011 = this->dotGradient(gradients, v0 + Indices(0, 1, 1), v);
    const Float x100 = this->dotGradient(gradients, v0 + Indices(1, 0, 0), v);
    const Float x101 = this->dotGradient(gradients, v0 + Indices(1, 0, 1), v);
    const Float x110 = this->dotGradient(gradients, v0 + Indices(1, 1, 0), v);
    const Float x111 = this->dotGradient(gradients, v0 + Indices(1, 1, 1), v);

    const Float x00 = lerp(x000, x001, dv[Z]);
    const Float x01 = lerp(x010, x011, dv[Z]);
    const Float x10 = lerp(x100, x101, dv[Z]);
    const Float x11 = lerp(x110, x111, dv[Z]);

    const Float x0 = lerp(x00, x01, dv[Y]);
    const Float x1 = lerp(x10, x11, dv[Y]);

    return lerp(x0, x1, dv[X]);
}

Float NoiseQuantityIc::dotGradient(const Grid<Vector>& gradients, const Indices& i, const Vector& v) const {
    const Vector& dv = Vector(i) - v;
    const Indices is(
        positiveMod(i[X], GRID_DIMS[X]), positiveMod(i[Y], GRID_DIMS[Y]), positiveMod(i[Z], GRID_DIMS[Z]));

    return dot(dv, gradients[is]);
}


static JobRegistrar sRegisterNoise(
    "Perlin noise",
    "noise",
    "initial conditions",
    [](const std::string& name) { return makeAuto<NoiseQuantityIc>(name); },
    "Perturbs selected quantity of the input body using a noise function.");

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
    : IParticleJob(name) {
    settings.addEntries(overrides);
}

VirtualSettings NBodyIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& particleCat = connector.addCategory("Particles");
    particleCat.connect<int>("Particle count", settings, NBodySettingsId::PARTICLE_COUNT);

    VirtualSettings::Category& distributionCat = connector.addCategory("Distribution");
    distributionCat.connect<Float>("Domain radius [km]", settings, NBodySettingsId::DOMAIN_RADIUS)
        .setUnits(1.e3_f);
    distributionCat.connect<Float>("Radial exponent", settings, NBodySettingsId::RADIAL_PROFILE);
    distributionCat.connect<Float>("Height scale", settings, NBodySettingsId::HEIGHT_SCALE);
    distributionCat.addEntry("min_size",
        makeEntry(settings, NBodySettingsId::POWER_LAW_INTERVAL, "Minimal size [m]", IntervalBound::LOWER));
    distributionCat.addEntry("max_size",
        makeEntry(settings, NBodySettingsId::POWER_LAW_INTERVAL, "Maximal size [m]", IntervalBound::UPPER));

    distributionCat.connect<Float>("Power-law exponent", settings, NBodySettingsId::POWER_LAW_EXPONENT);

    VirtualSettings::Category& dynamicsCat = connector.addCategory("Dynamics");
    dynamicsCat.connect<Float>("Total mass [M_earth]", settings, NBodySettingsId::TOTAL_MASS)
        .setUnits(Constants::M_earth);
    distributionCat.connect<Float>("Velocity multiplier", settings, NBodySettingsId::VELOCITY_MULTIPLIER);
    distributionCat
        .connect<Float>("Velocity dispersion [km/s]", settings, NBodySettingsId::VELOCITY_DISPERSION)
        .setUnits(1.e3_f);

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
    PointCloud cloud(radius / 10);
    Size bailoutCounter = 0;
    const Float sep = 1._f;
    const Size reportStep = max(particleCnt / 1000, 1u);
    do {
        Vector v = sampleSphere(radius, radialExponent, *rng);
        v[Z] *= heightScale;
        v[H] = sfd(rng(3));

        // check for intersections
        if (cloud.getClosePointsCount(v, sep * v[H]) > 0) {
            // discard
            bailoutCounter++;
            continue;
        }
        cloud.push(v);
        bailoutCounter = 0;

        if (cloud.size() % reportStep == reportStep - 1) {
            Statistics stats;
            stats.set(StatisticsId::RELATIVE_PROGRESS, Float(cloud.size()) / particleCnt);
            callbacks.onTimeStep(Storage(), stats);

            if (callbacks.shouldAbortRun()) {
                return;
            }
        }

    } while (cloud.size() < particleCnt && bailoutCounter < 1000);

    // assign masses
    Array<Vector> positions = cloud.array();
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
        SPH_ASSERT(masses[i] > 0._f);

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

static JobRegistrar sRegisterNBodyIc(
    "N-body ICs",
    "initial conditions",
    [](const std::string& name) { return makeAuto<NBodyIc>(name); },
    "Creates a spherical or ellipsoidal cloud of particles.");


// ----------------------------------------------------------------------------------------------------------
// PolytropicStarICs
// ----------------------------------------------------------------------------------------------------------

PolytropeIc::PolytropeIc(const std::string& name)
    : IParticleJob(name) {}

VirtualSettings PolytropeIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& starCat = connector.addCategory("Star parameters");
    starCat.connect("Particle count", "particleCnt", particleCnt);
    starCat.connect("Distribution", "distribution", distId);
    starCat.connect("Radius [km]", "radius", radius).setUnits(1.e3_f);
    starCat.connect("Minimal density [kg/m^3]", "rho_min", rho_min);
    starCat.connect("Polytrope index", "polytrope_index", n);

    return connector;
}

void PolytropeIc::evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<IMaterial> material = this->getInput<IMaterial>("material");
    material->setParam(BodySettingsId::ADIABATIC_INDEX, (n + 1._f) / n);
    material->setParam(BodySettingsId::DENSITY_RANGE, Interval(rho_min, INFTY));

    /// \todo to settings?
    material->setParam(BodySettingsId::SMOOTHING_LENGTH_ETA, eta);

    SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);

    BodySettings body;
    body.set(BodySettingsId::INITIAL_DISTRIBUTION, distId);
    AutoPtr<IDistribution> distribution = Factory::getDistribution(body);
    const Float rho0 = material->getParam<Float>(BodySettingsId::DENSITY);
    const Float mass = sphereVolume(radius) * rho0;
    Storage storage = Stellar::generateIc(scheduler, material, *distribution, particleCnt, radius, mass);

    result = makeShared<ParticleData>();
    result->storage = std::move(storage);
}

static JobRegistrar sRegisterPolytropeIc(
    "polytrope ICs",
    "initial conditions",
    [](const std::string& name) { return makeAuto<PolytropeIc>(name); },
    "Creates a spherical star or planet using the polytrope model.");

// ----------------------------------------------------------------------------------------------------------
// IsothermalSphereICs
// ----------------------------------------------------------------------------------------------------------

IsothermalSphereIc::IsothermalSphereIc(const std::string& name)
    : IParticleJob(name) {}

VirtualSettings IsothermalSphereIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& sphereCat = connector.addCategory("Sphere");
    sphereCat.connect("Particle count", "particleCnt", particleCnt);
    sphereCat.connect("Radius [km]", "radius", radius).setUnits(1.e3_f);
    sphereCat.connect("Central density [kg/m^3]", "density", centralDensity);
    sphereCat.connect("Central energy [J/kg]", "energy", centralEnergy);
    sphereCat.connect("Adiabatic index []", "gamma", gamma);

    return connector;
}

void IsothermalSphereIc::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    BodySettings body;
    body.set(BodySettingsId::DENSITY, centralDensity);
    body.set(BodySettingsId::ENERGY, centralEnergy);
    body.set(BodySettingsId::ADIABATIC_INDEX, gamma);
    SharedPtr<IMaterial> material = makeShared<EosMaterial>(body);
    Storage storage(material);
    DiehlParams params;
    Float r0 = 0.1_f * radius;
    params.numOfIters = 50;
    params.onIteration = DiehlReporter(callbacks, params.numOfIters);
    params.particleDensity = [r0](const Vector& r) {
        // does not have to be normalized
        return 1._f / (1._f + getSqrLength(r) / sqr(r0));
    };

    DiehlDistribution dist(params);
    SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);
    SphericalDomain domain(Vector(0._f), radius);
    storage.insert(QuantityId::POSITION, OrderEnum::SECOND, dist.generate(*scheduler, particleCnt, domain));
    const Float K = (gamma - 1._f) * centralEnergy;
    const Float M_tot = 2._f * PI * K * radius / Constants::gravity;
    storage.insert(QuantityId::MASS, OrderEnum::ZERO, M_tot / particleCnt);
    storage.insert(QuantityId::ENERGY, OrderEnum::FIRST, centralEnergy);
    storage.insert(QuantityId::DENSITY, OrderEnum::FIRST, centralEnergy);
    ArrayView<Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    const Float kingRadius = sqrt(9._f * K / (4._f * PI * Constants::gravity * centralDensity));
    for (Size i = 0; i < rho.size(); ++i) {
        rho[i] = centralDensity / (1._f + getSqrLength(r[i]) / sqr(kingRadius));
    }

    MaterialInitialContext context(global);
    material->create(storage, context);

    result = makeShared<ParticleData>();
    result->storage = std::move(storage);
}


static JobRegistrar sRegisterIsothermalSphereIc(
    "isothermal sphere ICs",
    "star ICs",
    "initial conditions",
    [](const std::string& name) { return makeAuto<IsothermalSphereIc>(name); },
    "Creates a single isothermal sphere.");

// ----------------------------------------------------------------------------------------------------------
// GalaxyICs
// ----------------------------------------------------------------------------------------------------------

extern template class Settings<GalaxySettingsId>;

GalaxyIc::GalaxyIc(const std::string& name, const GalaxySettings& overrides)
    : IParticleJob(name) {
    settings.addEntries(overrides);
}

VirtualSettings GalaxyIc::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& diskCat = connector.addCategory("Disk");
    diskCat.connect<int>("Disk particle count", settings, GalaxySettingsId::DISK_PARTICLE_COUNT);
    diskCat.connect<Float>("Disk radial scale", settings, GalaxySettingsId::DISK_RADIAL_SCALE);
    diskCat.connect<Float>("Disk radial cutoff", settings, GalaxySettingsId::DISK_RADIAL_CUTOFF);
    diskCat.connect<Float>("Disk vertical scale", settings, GalaxySettingsId::DISK_VERTICAL_SCALE);
    diskCat.connect<Float>("Disk vertical cutoff", settings, GalaxySettingsId::DISK_VERTICAL_CUTOFF);
    diskCat.connect<Float>("Disk mass", settings, GalaxySettingsId::DISK_MASS);
    diskCat.connect<Float>("Toomre Q parameter", settings, GalaxySettingsId::DISK_TOOMRE_Q);

    VirtualSettings::Category& haloCat = connector.addCategory("Halo");
    haloCat.connect<int>("Halo particle count", settings, GalaxySettingsId::HALO_PARTICLE_COUNT);
    haloCat.connect<Float>("Halo scale length", settings, GalaxySettingsId::HALO_SCALE_LENGTH);
    haloCat.connect<Float>("Halo cutoff", settings, GalaxySettingsId::HALO_CUTOFF);
    haloCat.connect<Float>("Halo gamma", settings, GalaxySettingsId::HALO_GAMMA);
    haloCat.connect<Float>("Halo mass", settings, GalaxySettingsId::HALO_MASS);

    VirtualSettings::Category& bulgeCat = connector.addCategory("Bulge");
    bulgeCat.connect<int>("Bulge particle count", settings, GalaxySettingsId::BULGE_PARTICLE_COUNT);
    bulgeCat.connect<Float>("Bulge scale length", settings, GalaxySettingsId::BULGE_SCALE_LENGTH);
    bulgeCat.connect<Float>("Bulge cutoff", settings, GalaxySettingsId::BULGE_CUTOFF);
    bulgeCat.connect<Float>("Bulge mass", settings, GalaxySettingsId::BULGE_MASS);

    VirtualSettings::Category& particleCat = connector.addCategory("Particles");
    particleCat.connect<Float>("Particle radius", settings, GalaxySettingsId::PARTICLE_RADIUS);

    return connector;
}

class GalaxyCallbacks : public Galaxy::IProgressCallbacks {
private:
    IRunCallbacks& run;

public:
    explicit GalaxyCallbacks(IRunCallbacks& run)
        : run(run) {}

    struct Cancelled {};

    virtual void onPart(const Storage& storage, const Size partId, const Size numParts) const override {
        if (storage.empty()) {
            SPH_ASSERT(partId == 0);
            return;
        }

        Statistics stats;
        stats.set(StatisticsId::RELATIVE_PROGRESS, Float(partId) / Float(numParts));
        stats.set(StatisticsId::RUN_TIME, 0._f);

        if (partId == 1) {
            run.onSetUp(storage, stats);
        }
        run.onTimeStep(storage, stats);

        if (run.shouldAbortRun()) {
            throw Cancelled{};
        }
    }
};

void GalaxyIc::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    Storage storage;
    try {
        storage = Galaxy::generateIc(global, settings, GalaxyCallbacks(callbacks));
    } catch (const GalaxyCallbacks::Cancelled&) {
        return;
    }

    result = makeShared<ParticleData>();
    result->storage = std::move(storage);

    /// \todo generalize units
    result->overrides.set(RunSettingsId::GRAVITY_CONSTANT, 1._f);
}

static JobRegistrar sRegisterGalaxyIc(
    "galaxy ICs",
    "initial conditions",
    [](const std::string& name) { return makeAuto<GalaxyIc>(name); },
    "Creates a single galaxy.");

NAMESPACE_SPH_END
