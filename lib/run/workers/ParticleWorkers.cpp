#include "run/workers/ParticleWorkers.h"
#include "io/LogWriter.h"
#include "io/Logger.h"
#include "objects/geometry/Sphere.h"
#include "post/Analysis.h"
#include "quantities/Quantity.h"
#include "run/IRun.h"
#include "sph/Materials.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "system/Settings.impl.h"

NAMESPACE_SPH_BEGIN

//-----------------------------------------------------------------------------------------------------------
// CachedParticlesWorker
//-----------------------------------------------------------------------------------------------------------

CachedParticlesWorker::CachedParticlesWorker(const std::string& name, const Storage& storage)
    : IParticleWorker(name) {
    if (!storage.empty()) {
        cached.storage = storage.clone(VisitorEnum::ALL_BUFFERS);
        useCached = true;
    }
}

VirtualSettings CachedParticlesWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& cacheCat = connector.addCategory("Caching");
    cacheCat.connect("Use cached data", "use_cache", useCached)
        .connect("Switch to cached on eval", "do_cache", doSwitch);

    return connector;
}

void CachedParticlesWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = makeShared<ParticleData>();
    if (useCached) {
        result->storage = cached.storage.clone(VisitorEnum::ALL_BUFFERS);
        result->overrides = cached.overrides;
        result->stats = cached.stats;
    } else {
        SharedPtr<ParticleData> input = this->getInput<ParticleData>("particles");
        cached.storage = input->storage.clone(VisitorEnum::ALL_BUFFERS);
        cached.overrides = input->overrides;
        cached.stats = input->stats;
        result = input;

        /// \todo how to signal the grid?
        if (doSwitch) {
            useCached = true;
        }
    }
}

static WorkerRegistrar sRegisterCache("cache", "particle operators", [](const std::string& name) {
    return makeAuto<CachedParticlesWorker>(name);
});

//-----------------------------------------------------------------------------------------------------------
// MergeParticlesWorker
//-----------------------------------------------------------------------------------------------------------

VirtualSettings MergeParticlesWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& cat = connector.addCategory("Merging");
    cat.connect("Offset [km]", "offset", offset, 1.e3_f);
    cat.connect("Add velocity [km/s]", "velocity", velocity, 1.e3_f);
    cat.connect("Move to COM", "com", moveToCom);
    cat.connect("Make flags unique", "unique_flags", uniqueFlags);

    return connector;
}

void MergeParticlesWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& callbacks) {
    SharedPtr<ParticleData> input1 = this->getInput<ParticleData>("particles A");
    SharedPtr<ParticleData> input2 = this->getInput<ParticleData>("particles B");

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = input2->storage.getAll<Vector>(QuantityId::POSITION);
    offset[H] = 0._f; // can contain garbage
    for (Size i = 0; i < r.size(); ++i) {
        r[i] += offset;
        v[i] += velocity;
    }

    if (uniqueFlags) {
        ArrayView<Size> flags1 = input1->storage.getValue<Size>(QuantityId::FLAG);
        ArrayView<Size> flags2 = input2->storage.getValue<Size>(QuantityId::FLAG);

        const Size flagOffset = Size(*std::max_element(flags1.begin(), flags1.end())) + 1;
        for (Size i = 0; i < flags2.size(); ++i) {
            flags2[i] += flagOffset;
        }
    }

    input1->storage.merge(std::move(input2->storage));

    if (moveToCom) {
        ArrayView<Vector> v, dv;
        tie(r, v, dv) = input1->storage.getAll<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = input1->storage.getValue<Float>(QuantityId::MASS);
        moveToCenterOfMassSystem(m, r);
        moveToCenterOfMassSystem(m, v);
    }

    result = input1;
    callbacks.onSetUp(result->storage, result->stats);
}


static WorkerRegistrar sRegisterParticleMerge("merge", "particle operators", [](const std::string& name) {
    return makeAuto<MergeParticlesWorker>(name);
});


//-----------------------------------------------------------------------------------------------------------
// TransformParticlesWorker
//-----------------------------------------------------------------------------------------------------------

VirtualSettings TransformParticlesWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& posCat = connector.addCategory("Positions");
    posCat.connect("Translate [km]", "offset", positions.offset, 1.e3_f);
    posCat.connect("Yaw angle [deg]", "yaw", positions.angles[0], DEG_TO_RAD);
    posCat.connect("Pitch angle [deg]", "pitch", positions.angles[1], DEG_TO_RAD);
    posCat.connect("Roll angle [deg]", "roll", positions.angles[2], DEG_TO_RAD);

    VirtualSettings::Category& velCat = connector.addCategory("Velocities");
    velCat.connect("Add velocity [km/s]", "velocity", velocities.offset, 1.e3_f);

    return connector;
}

void TransformParticlesWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& callbacks) {
    result = this->getInput<ParticleData>("particles");

    AffineMatrix positionTm = AffineMatrix::rotateX(positions.angles[0]) *
                              AffineMatrix::rotateY(positions.angles[1]) *
                              AffineMatrix::rotateZ(positions.angles[2]);
    positionTm.translate(positions.offset);

    AffineMatrix velocityTm = AffineMatrix::identity();
    velocityTm.translate(velocities.offset);

    ArrayView<Vector> r = result->storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Vector> v = result->storage.getDt<Vector>(QuantityId::POSITION);

    for (Size i = 0; i < r.size(); ++i) {
        const Float h = r[i][H];
        r[i] = positionTm * r[i];
        r[i][H] = h;

        v[i] = velocityTm * v[i];
        v[i][H] = 0._f;
    }

    callbacks.onSetUp(result->storage, result->stats);
}


static WorkerRegistrar sRegisterParticleTransform("transform",
    "particle operators",
    [](const std::string& name) { return makeAuto<TransformParticlesWorker>(name); });

//-----------------------------------------------------------------------------------------------------------
// ChangeMaterialWorker
//-----------------------------------------------------------------------------------------------------------

static RegisterEnum<ChangeMaterialSubset> sSubsetType({
    { ChangeMaterialSubset::ALL, "all", "Change material of all particles." },
    { ChangeMaterialSubset::MATERIAL_ID,
        "material_id",
        "Change material of particles with specific material ID." },
    { ChangeMaterialSubset::INSIDE_DOMAIN, "inside_domain", "Change material of particles in given domain." },
});

VirtualSettings ChangeMaterialWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& cat = connector.addCategory("Change material");
    cat.connect("Subset", "subset", type).connect("Material ID", "mat_id", matId, [this] {
        return ChangeMaterialSubset(type) == ChangeMaterialSubset::MATERIAL_ID;
    });

    return connector;
}

void ChangeMaterialWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<ParticleData> input = this->getInput<ParticleData>("particles");
    SharedPtr<IMaterial> material = this->getInput<IMaterial>("material");

    switch (ChangeMaterialSubset(type)) {
    case ChangeMaterialSubset::ALL:
        for (Size i = 0; i < input->storage.getMaterialCnt(); ++i) {
            input->storage.setMaterial(i, material);
        }
        break;
    case ChangeMaterialSubset::MATERIAL_ID:
        input->storage.setMaterial(matId, material);
        break;
    case ChangeMaterialSubset::INSIDE_DOMAIN: {
        SharedPtr<IDomain> domain = this->getInput<IDomain>("domain");
        ArrayView<const Vector> r = input->storage.getValue<Vector>(QuantityId::POSITION);
        Array<Size> toChange, toKeep;
        for (Size i = 0; i < r.size(); ++i) {
            if (domain->contains(r[i])) {
                toChange.push(i);
            } else {
                toKeep.push(i);
            }
        }

        Storage changed = input->storage.clone(VisitorEnum::ALL_BUFFERS);
        changed.remove(toKeep, Storage::IndicesFlag::INDICES_SORTED);
        input->storage.remove(toChange, Storage::IndicesFlag::INDICES_SORTED);

        for (Size i = 0; i < changed.getMaterialCnt(); ++i) {
            changed.setMaterial(i, material);
        }
        input->storage.merge(std::move(changed));
        break;
    }
    }

    result = input;
}


static WorkerRegistrar sRegisterChangeMaterial("change material",
    "changer",
    "particle operators",
    [](const std::string& name) { return makeAuto<ChangeMaterialWorker>(name); });

//-----------------------------------------------------------------------------------------------------------
// CollisionGeometrySetup
//-----------------------------------------------------------------------------------------------------------

// clang-format off
template <>
AutoPtr<CollisionGeometrySettings> CollisionGeometrySettings::instance(new CollisionGeometrySettings{
    { CollisionGeometrySettingsId::IMPACTOR_OPTIMIZE,      "impactor.optimize",        true,
        "If true, some quantities of the impactor particles are not taken into account when computing the required "
        "time step. Otherwise, the time step might be unnecessarily too low, as the quantities in the impactor change "
        "rapidly. Note that this does not affect CFL criterion. It should be always set to false for collisions"
        "of similar-sized bodies."},
    { CollisionGeometrySettingsId::IMPACTOR_OFFSET,        "impactor.offset",          4._f,
        "Initial distance of the impactor from the target in units of smoothing length. The impactor should "
        "not be in contact with the target at the start of the simulation, so the value should be always larger "
        "than the radius of the selected kernel." },
    { CollisionGeometrySettingsId::IMPACT_SPEED,           "impact.speed",             5.e3_f,
        "Relative impact speed (or absolute speed of the impactor if center-of-mass system is set to false) "
        "in meters per second." },
    { CollisionGeometrySettingsId::IMPACT_ANGLE,           "impact.angle",             45._f,
        "Impact angle, i.e. angle between normal at the point of impact and the velocity vector of the impactor. "
        "It can be negative to simulate retrograde impact. The angle is in degrees. "},
    { CollisionGeometrySettingsId::CENTER_OF_MASS_FRAME,   "center_of_mass_frame",     false,
        "If true, colliding bodies are moved to the center-of-mass system, otherwise the target is located "
        "at origin and has zero velocity." },
});
// clang-format on

template class Settings<CollisionGeometrySettingsId>;

/// \brief Returns a sphere enclosing all particles in the storage.
///
/// Not necessarily the smallest sphere, but it is the smallest for spherical bodies.
static Sphere getBoundingSphere(const Storage& storage) {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    Sphere sphere(Vector(0._f), 0._f);
    for (Size i = 0; i < r.size(); ++i) {
        sphere.center() += r[i];
    }
    sphere.center() /= r.size();

    for (Size i = 0; i < r.size(); ++i) {
        sphere.radius() = max(sphere.radius(), getLength(r[i] - sphere.center()));
    }
    return sphere;
}

static void displace(Storage& storage, const Vector& offset) {
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    Vector fixedOffset = offset;
    fixedOffset[H] = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        r[i] += fixedOffset;
    }
}

CollisionGeometrySetup::CollisionGeometrySetup(const std::string& name,
    const CollisionGeometrySettings& overrides)
    : IParticleWorker(name) {
    geometry.addEntries(overrides);
}

VirtualSettings CollisionGeometrySetup::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& positionCat = connector.addCategory("Collision geometry");
    positionCat.connect<Float>("Impact angle [deg]", geometry, CollisionGeometrySettingsId::IMPACT_ANGLE)
        .connect<Float>("Impact velocity [km/s]", geometry, CollisionGeometrySettingsId::IMPACT_SPEED, 1.e3_f)
        .connect<Float>("Impactor offset [h]", geometry, CollisionGeometrySettingsId::IMPACTOR_OFFSET)
        .connect<bool>("Move to CoM frame", geometry, CollisionGeometrySettingsId::CENTER_OF_MASS_FRAME);

    return connector;
}

void CollisionGeometrySetup::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    Storage target = std::move(this->getInput<ParticleData>("target")->storage);
    Storage impactor = std::move(this->getInput<ParticleData>("impactor")->storage);
    ASSERT(target.isValid());
    ASSERT(impactor.isValid());

    const Sphere targetSphere = getBoundingSphere(target);
    const Sphere impactorSphere = getBoundingSphere(impactor);

    // move target to origin
    displace(target, -targetSphere.center());

    // move impactor to impact angle
    const Float impactorDistance = targetSphere.radius() + impactorSphere.radius();

    const Float h = target.getValue<Vector>(QuantityId::POSITION)[0][H];
    const Float phi = geometry.get<Float>(CollisionGeometrySettingsId::IMPACT_ANGLE) * DEG_TO_RAD;
    ASSERT(phi >= -PI && phi <= PI, phi);

    const Float offset = geometry.get<Float>(CollisionGeometrySettingsId::IMPACTOR_OFFSET);
    const Float x = impactorDistance * cos(phi) + offset * h;
    const Float y = impactorDistance * sin(phi);
    displace(impactor, -impactorSphere.center() + Vector(x, y, 0._f));

    const Float v_imp = geometry.get<Float>(CollisionGeometrySettingsId::IMPACT_SPEED);
    ArrayView<Vector> v = impactor.getDt<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < v.size(); ++i) {
        v[i][X] -= v_imp;
    }

    // renumber flags of impactor to separate the bodies
    if (target.has(QuantityId::FLAG) && impactor.has(QuantityId::FLAG)) {
        ArrayView<Size> targetFlags = target.getValue<Size>(QuantityId::FLAG);
        ArrayView<Size> impactorFlags = impactor.getValue<Size>(QuantityId::FLAG);
        const Size flagShift = *std::max_element(targetFlags.begin(), targetFlags.end()) + 1;
        for (Size i = 0; i < impactorFlags.size(); ++i) {
            impactorFlags[i] += flagShift;
        }
    }

    target.merge(std::move(impactor));

    if (geometry.get<bool>(CollisionGeometrySettingsId::CENTER_OF_MASS_FRAME)) {
        ArrayView<Float> m = target.getValue<Float>(QuantityId::MASS);
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = target.getAll<Vector>(QuantityId::POSITION);

        moveToCenterOfMassSystem(m, r);
        moveToCenterOfMassSystem(m, v);
    }

    // merge bodies to single storage
    result = makeShared<ParticleData>();
    result->storage = std::move(target);
}

static WorkerRegistrar sRegisterCollisionSetup("collision setup",
    "setup",
    "particle operators",
    [](const std::string& name) { return makeAuto<CollisionGeometrySetup>(name); });


// ----------------------------------------------------------------------------------------------------------
// SmoothedToSolidHandoff
// ----------------------------------------------------------------------------------------------------------

VirtualSettings SmoothedToSolidHandoff::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    return connector;
}

void SmoothedToSolidHandoff::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {

    // we don't need any material, so just pass some dummy
    Storage spheres(makeAuto<NullMaterial>(EMPTY_SETTINGS));
    Storage input = std::move(this->getInput<ParticleData>("particles")->storage);

    // clone required quantities
    spheres.insert<Vector>(
        QuantityId::POSITION, OrderEnum::SECOND, input.getValue<Vector>(QuantityId::POSITION).clone());
    spheres.getDt<Vector>(QuantityId::POSITION) = input.getDt<Vector>(QuantityId::POSITION).clone();
    spheres.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, input.getValue<Float>(QuantityId::MASS).clone());

    // radii handoff
    ArrayView<const Float> m = input.getValue<Float>(QuantityId::MASS);
    ArrayView<const Float> rho = input.getValue<Float>(QuantityId::DENSITY);
    ArrayView<Vector> r_sphere = spheres.getValue<Vector>(QuantityId::POSITION);
    ASSERT(r_sphere.size() == rho.size());
    for (Size i = 0; i < r_sphere.size(); ++i) {
        r_sphere[i][H] = cbrt(3._f * m[i] / (4._f * PI * rho[i]));
    }

    // remove all sublimated particles
    Array<Size> toRemove;
    ArrayView<const Float> u = input.getValue<Float>(QuantityId::ENERGY);
    for (Size matId = 0; matId < input.getMaterialCnt(); ++matId) {
        MaterialView mat = input.getMaterial(matId);
        const Float u_max = mat->getParam<Float>(BodySettingsId::TILLOTSON_SUBLIMATION);
        for (Size i : mat.sequence()) {
            if (u[i] > u_max) {
                toRemove.push(i);
            }
        }
    }
    spheres.remove(toRemove);

    // move to COM system
    ArrayView<Vector> v_sphere, dummy;
    tie(r_sphere, v_sphere, dummy) = spheres.getAll<Vector>(QuantityId::POSITION);
    m = input.getValue<Float>(QuantityId::MASS);
    moveToCenterOfMassSystem(m, v_sphere);
    moveToCenterOfMassSystem(m, r_sphere);

    result = makeShared<ParticleData>();
    result->storage = std::move(spheres);
}

static WorkerRegistrar sRegisterHandoff("smoothed-to-solid handoff",
    "handoff",
    "particle operators",
    [](const std::string& name) { return makeAuto<SmoothedToSolidHandoff>(name); });


// ----------------------------------------------------------------------------------------------------------
// ExtractComponentWorker
// ----------------------------------------------------------------------------------------------------------

VirtualSettings ExtractComponentWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& category = connector.addCategory("Component");
    category.connect("Component index", "index", componentIdx)
        .connect("Connectivity factor", "factor", factor)
        .connect("Move to CoM", "center", center);
    return connector;
}

void ExtractComponentWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    Storage storage = std::move(this->getInput<ParticleData>("particles")->storage);

    // allow using this for storage without masses --> add ad hoc mass if it's missing
    if (!storage.has(QuantityId::MASS)) {
        storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 1._f);
    }

    Array<Size> components;
    Post::findComponents(storage, factor, Post::ComponentFlag::SORT_BY_MASS, components);

    Array<Size> toRemove;
    for (Size i = 0; i < components.size(); ++i) {
        if (int(components[i]) != componentIdx) {
            // not LR
            toRemove.push(i);
        }
    }
    storage.remove(toRemove, Storage::IndicesFlag::INDICES_SORTED);

    if (center) {
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        moveToCenterOfMassSystem(m, r);
        moveToCenterOfMassSystem(m, v);
    }

    result = makeShared<ParticleData>();
    result->storage = std::move(storage);
}

static WorkerRegistrar sRegisterExtractComponent("extract component",
    "extractor",
    "particle operators",
    [](const std::string& name) { return makeAuto<ExtractComponentWorker>(name); });

// ----------------------------------------------------------------------------------------------------------
// ExtractComponentWorker
// ----------------------------------------------------------------------------------------------------------

VirtualSettings ExtractParticlesInDomainWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& category = connector.addCategory("Misc");
    category.connect("Move to CoM", "center", center);
    return connector;
}

void ExtractParticlesInDomainWorker::evaluate(const RunSettings& UNUSED(global),
    IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<ParticleData> data = this->getInput<ParticleData>("particles");
    SharedPtr<IDomain> domain = this->getInput<IDomain>("domain");
    Storage& storage = data->storage;

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    Array<Size> toRemove;
    for (Size i = 0; i < r.size(); ++i) {
        if (!domain->contains(r[i])) {
            toRemove.push(i);
        }
    }
    storage.remove(toRemove, Storage::IndicesFlag::INDICES_SORTED);

    if (center) {
        ArrayView<Vector> v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        moveToCenterOfMassSystem(m, r);
        moveToCenterOfMassSystem(m, v);
    }

    result = data;
}

static WorkerRegistrar sRegisterExtractInDomain("extract particles in domain",
    "extractor",
    "particle operators",
    [](const std::string& name) { return makeAuto<ExtractParticlesInDomainWorker>(name); });

// ----------------------------------------------------------------------------------------------------------
// EmplaceComponentsAsFlagsWorker
// ----------------------------------------------------------------------------------------------------------

VirtualSettings EmplaceComponentsAsFlagsWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& category = connector.addCategory("Component");
    category.connect("Connectivity factor", "factor", factor);
    return connector;
}

void EmplaceComponentsAsFlagsWorker::evaluate(const RunSettings& UNUSED(global),
    IRunCallbacks& UNUSED(callbacks)) {
    Storage fragments = std::move(this->getInput<ParticleData>("fragments")->storage);

    Array<Size> components;
    Post::findComponents(fragments, factor, Post::ComponentFlag::SORT_BY_MASS, components);

    Storage original = std::move(this->getInput<ParticleData>("original")->storage);
    if (!original.has(QuantityId::FLAG)) {
        original.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, 0);
    }
    ArrayView<Size> flags = original.getValue<Size>(QuantityId::FLAG);
    if (flags.size() != components.size()) {
        throw InvalidSetup("Inputs have different numbers of particles");
    }

    for (Size i = 0; i < flags.size(); ++i) {
        flags[i] = components[i];
    }

    result = makeShared<ParticleData>();
    result->storage = std::move(original);
}

static WorkerRegistrar sRegisterEmplaceComponents("emplace components",
    "emplacer",
    "particle operators",
    [](const std::string& name) { return makeAuto<EmplaceComponentsAsFlagsWorker>(name); });


// ----------------------------------------------------------------------------------------------------------
// SubsampleWorker
// ----------------------------------------------------------------------------------------------------------

VirtualSettings SubsampleWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& category = connector.addCategory("Subsampling");
    category.connect("Fraction", "fraction", fraction);
    return connector;
}

void SubsampleWorker::evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<ParticleData> input = this->getInput<ParticleData>("particles");
    AutoPtr<IRng> rng = Factory::getRng(global);

    std::set<Size> generated;
    const Size particleCnt = input->storage.getParticleCnt();
    const Size targetCnt = clamp(Size((1._f - fraction) * particleCnt), 0u, particleCnt - 1);
    while (generated.size() < targetCnt) {
        generated.insert(Size(rng() * particleCnt));
    }
    Array<Size> toRemove;
    for (Size i : generated) {
        toRemove.push(i);
    }

    input->storage.remove(toRemove, Storage::IndicesFlag::INDICES_SORTED);

    ArrayView<Float> m = input->storage.getValue<Float>(QuantityId::MASS);
    for (Size i = 0; i < m.size(); ++i) {
        m[i] /= fraction;
    }

    result = input;
}

static WorkerRegistrar sRegisterSubsampler("subsampler", "particle operators", [](const std::string& name) {
    return makeAuto<SubsampleWorker>(name);
});


NAMESPACE_SPH_END
