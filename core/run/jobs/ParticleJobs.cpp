#include "run/jobs/ParticleJobs.h"
#include "io/LogWriter.h"
#include "io/Logger.h"
#include "objects/geometry/Sphere.h"
#include "post/Analysis.h"
#include "post/Compare.h"
#include "post/TwoBody.h"
#include "quantities/Quantity.h"
#include "run/IRun.h"
#include "sph/Materials.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "system/Settings.impl.h"

NAMESPACE_SPH_BEGIN

static void renumberFlags(const Storage& main, Storage& other) {
    if (!main.has(QuantityId::FLAG) || !other.has(QuantityId::FLAG)) {
        return;
    }

    ArrayView<const Size> flags1 = main.getValue<Size>(QuantityId::FLAG);
    const Size offset = *std::max_element(flags1.begin(), flags1.end()) + 1;

    ArrayView<Size> flags2 = other.getValue<Size>(QuantityId::FLAG);
    for (Size& f : flags2) {
        f += offset;
    }
}

//-----------------------------------------------------------------------------------------------------------
// JoinParticlesJob
//-----------------------------------------------------------------------------------------------------------

VirtualSettings JoinParticlesJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& cat = connector.addCategory("Merging");
    cat.connect("Offset [km]", "offset", offset).setUnits(1.e3_f);
    cat.connect("Add velocity [km/s]", "velocity", velocity).setUnits(1.e3_f);
    cat.connect("Move to COM", "com", moveToCom)
        .setTooltip(
            "If true, the particles are moved so that their center of mass lies at the origin and their "
            "velocities are modified so that the total momentum is zero.");
    cat.connect("Make flags unique", "unique_flags", uniqueFlags)
        .setTooltip(
            "If true, the particle flags of the second input state are renumbered to avoid overlap with "
            "flags of the first input. This is necessary to properly separate the input bodies.");

    return connector;
}

void JoinParticlesJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& callbacks) {
    SharedPtr<ParticleData> input1 = this->getInput<ParticleData>("particles A");
    SharedPtr<ParticleData> input2 = this->getInput<ParticleData>("particles B");

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = input2->storage.getAll<Vector>(QuantityId::POSITION);
    offset[H] = 0._f; // can contain garbage
    velocity[H] = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        r[i] += offset;
        v[i] += velocity;
    }

    if (uniqueFlags) {
        renumberFlags(input1->storage, input2->storage);
    }

    input1->storage.merge(std::move(input2->storage));

    if (moveToCom) {
        moveToCenterOfMassSystem(input1->storage);
    }

    result = input1;
    callbacks.onSetUp(result->storage, result->stats);
}


static JobRegistrar sRegisterParticleJoin(
    "join",
    "particle operators",
    [](const std::string& name) { return makeAuto<JoinParticlesJob>(name); },
    "Simply adds particles from two inputs into a single particle state. Optionally, positions and "
    "velocities of particles in the second state may be shifted.");

//-----------------------------------------------------------------------------------------------------------
// OrbitParticlesJob
//-----------------------------------------------------------------------------------------------------------

VirtualSettings OrbitParticlesJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& cat = connector.addCategory("Ellipse");
    cat.connect("semi-major axis [km]", "a", a).setUnits(1.e3_f);
    cat.connect("eccentricity []", "e", e);
    cat.connect("initial proper anomaly [deg]", "v", v).setUnits(DEG_TO_RAD);

    return connector;
}

void OrbitParticlesJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& callbacks) {
    SharedPtr<ParticleData> input1 = this->getInput<ParticleData>("particles A");
    SharedPtr<ParticleData> input2 = this->getInput<ParticleData>("particles B");

    const Float u = Kepler::trueAnomalyToEccentricAnomaly(v, e);
    ArrayView<const Float> m1 = input1->storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Float> m2 = input2->storage.getValue<Float>(QuantityId::MASS);

    const Float m_tot =
        std::accumulate(m1.begin(), m1.end(), 0._f) + std::accumulate(m2.begin(), m2.end(), 0._f);
    const Float n = Kepler::meanMotion(a, m_tot);
    const Vector dr = Kepler::position(a, e, u);
    const Vector dv = Kepler::velocity(a, e, u, n);

    ArrayView<Vector> r = input2->storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Vector> v = input2->storage.getDt<Vector>(QuantityId::POSITION);

    for (Size i = 0; i < r.size(); ++i) {
        r[i] += dr;
        v[i] += dv;
    }

    renumberFlags(input1->storage, input2->storage);
    input1->storage.merge(std::move(input2->storage));
    input2->storage.removeAll();

    moveToCenterOfMassSystem(input1->storage);

    result = input1;
    callbacks.onSetUp(result->storage, result->stats);
}


static JobRegistrar sRegisterParticleOrbit(
    "orbit",
    "particle operators",
    [](const std::string& name) { return makeAuto<OrbitParticlesJob>(name); },
    "Puts two input bodies on an elliptical trajectory around their common center of gravity. The orbit is "
    "defined by the semi-major axis and the eccentricity and it lies in the z=0 plane.");


//-----------------------------------------------------------------------------------------------------------
// JoinParticlesJob
//-----------------------------------------------------------------------------------------------------------

VirtualSettings MultiJoinParticlesJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& defCat = connector.addCategory("Slots");
    defCat.connect("Number of slots", "slot_cnt", slotCnt);

    VirtualSettings::Category& cat = connector.addCategory("Merging");
    cat.connect("Move to COM", "com", moveToCom)
        .setTooltip(
            "If true, the particles are moved so that their center of mass lies at the origin and their "
            "velocities are modified so that the total momentum is zero.");
    cat.connect("Make flags unique", "unique_flags", uniqueFlags)
        .setTooltip(
            "If true, the particle flags of the states are renumbered to avoid overlap with "
            "flags of other inputs. This is necessary to properly separate the input bodies.");

    return connector;
}

void MultiJoinParticlesJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& callbacks) {
    SharedPtr<ParticleData> main = this->getInput<ParticleData>("particles 1");
    for (int i = 1; i < slotCnt; ++i) {
        SharedPtr<ParticleData> other = this->getInput<ParticleData>("particles " + std::to_string(i + 1));
        if (uniqueFlags) {
            renumberFlags(main->storage, other->storage);
        }
        main->storage.merge(std::move(other->storage));
        other->storage.removeAll();
    }

    if (moveToCom) {
        moveToCenterOfMassSystem(main->storage);
    }

    result = main;
    callbacks.onSetUp(result->storage, result->stats);
}

static JobRegistrar sRegisterParticleMultiJoin(
    "multi join",
    "particle operators",
    [](const std::string& name) { return makeAuto<MultiJoinParticlesJob>(name); },
    "Joins multiple particle sources into a single states.");


//-----------------------------------------------------------------------------------------------------------
// TransformParticlesJob
//-----------------------------------------------------------------------------------------------------------

VirtualSettings TransformParticlesJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& posCat = connector.addCategory("Positions");
    posCat.connect("Translate [km]", "offset", positions.offset).setUnits(1.e3_f);
    posCat.connect("Yaw angle [deg]", "yaw", positions.angles[0]).setUnits(DEG_TO_RAD);
    posCat.connect("Pitch angle [deg]", "pitch", positions.angles[1]).setUnits(DEG_TO_RAD);
    posCat.connect("Roll angle [deg]", "roll", positions.angles[2]).setUnits(DEG_TO_RAD);

    VirtualSettings::Category& velCat = connector.addCategory("Velocities");
    velCat.connect("Add velocity [km/s]", "velocity_offset", velocities.offset).setUnits(1.e3_f);
    velCat.connect("Add spin [rev/day]", "spin", spin).setUnits(2._f * PI / (3600._f * 24._f));
    velCat.connect("Multiplier", "velocity_mult", velocities.mult);

    return connector;
}

void TransformParticlesJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& callbacks) {
    result = this->getInput<ParticleData>("particles");

    AffineMatrix rotator = AffineMatrix::rotateX(positions.angles[0]) *
                           AffineMatrix::rotateY(positions.angles[1]) *
                           AffineMatrix::rotateZ(positions.angles[2]);

    AffineMatrix positionTm = rotator;
    positionTm.translate(positions.offset);

    // using same TM for positions and velocities is correct for orthogonal matrices
    AffineMatrix velocityTm = rotator * AffineMatrix::scale(Vector(velocities.mult));
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

    if (spin != Vector(0._f)) {
        const Vector r_com = getCenterOfMass(result->storage);
        for (Size i = 0; i < r.size(); ++i) {
            v[i] += cross(spin, r[i] - r_com);
        }
    }

    callbacks.onSetUp(result->storage, result->stats);
}


static JobRegistrar sRegisterParticleTransform(
    "transform",
    "particle operators",
    [](const std::string& name) { return makeAuto<TransformParticlesJob>(name); },
    "Modifies positions and velocities of the input particles.");

//-----------------------------------------------------------------------------------------------------------
// CenterParticlesJob
//-----------------------------------------------------------------------------------------------------------

VirtualSettings CenterParticlesJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& centerCat = connector.addCategory("Center");
    centerCat.connect("Move to CoM", "positions", centerPositions);
    centerCat.connect("Set zero momentum", "velocities", centerVelocities);

    return connector;
}

void CenterParticlesJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& callbacks) {
    result = this->getInput<ParticleData>("particles");
    Storage& storage = result->storage;

    Array<Float> m;
    if (storage.has(QuantityId::MASS)) {
        m = storage.getValue<Float>(QuantityId::MASS).clone();
    } else {
        m.resize(storage.getParticleCnt());
        m.fill(1._f);
    }
    if (centerPositions) {
        moveToCenterOfMassSystem(m, storage.getValue<Vector>(QuantityId::POSITION));
    }
    if (centerVelocities) {
        moveToCenterOfMassSystem(m, storage.getDt<Vector>(QuantityId::POSITION));
    }

    callbacks.onSetUp(result->storage, result->stats);
}


static JobRegistrar sRegisterCenterTransform(
    "center",
    "particle operators",
    [](const std::string& name) { return makeAuto<CenterParticlesJob>(name); },
    "Moves particle positions and/or velocities to center-of-mass frame.");


//-----------------------------------------------------------------------------------------------------------
// ChangeMaterialJob
//-----------------------------------------------------------------------------------------------------------

static RegisterEnum<ChangeMaterialSubset> sSubsetType({
    { ChangeMaterialSubset::ALL, "all", "Change material of all particles." },
    { ChangeMaterialSubset::MATERIAL_ID,
        "material_id",
        "Change material of particles with specific material ID." },
    { ChangeMaterialSubset::INSIDE_DOMAIN, "inside_domain", "Change material of particles in given domain." },
});

VirtualSettings ChangeMaterialJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& cat = connector.addCategory("Change material");
    cat.connect("Subset", "subset", type);
    cat.connect("Material ID", "mat_id", matId).setEnabler([this] {
        return ChangeMaterialSubset(type) == ChangeMaterialSubset::MATERIAL_ID;
    });

    return connector;
}

void ChangeMaterialJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
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


static JobRegistrar sRegisterChangeMaterial(
    "change material",
    "changer",
    "particle operators",
    [](const std::string& name) { return makeAuto<ChangeMaterialJob>(name); },
    "Changes the material of all or a subset of the input particles.");

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
    : IParticleJob(name) {
    geometry.addEntries(overrides);
}

VirtualSettings CollisionGeometrySetup::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& positionCat = connector.addCategory("Collision geometry");
    positionCat.connect<Float>("Impact angle [deg]", geometry, CollisionGeometrySettingsId::IMPACT_ANGLE);
    positionCat.connect<Float>("Impact velocity [km/s]", geometry, CollisionGeometrySettingsId::IMPACT_SPEED)
        .setUnits(1.e3_f);
    positionCat.connect<Float>("Impactor offset [h]", geometry, CollisionGeometrySettingsId::IMPACTOR_OFFSET);
    positionCat.connect<bool>(
        "Move to CoM frame", geometry, CollisionGeometrySettingsId::CENTER_OF_MASS_FRAME);

    return connector;
}

void CollisionGeometrySetup::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    Storage target = std::move(this->getInput<ParticleData>("target")->storage);
    Storage impactor = std::move(this->getInput<ParticleData>("impactor")->storage);
    SPH_ASSERT(target.isValid());
    SPH_ASSERT(impactor.isValid());

    const Sphere targetSphere = getBoundingSphere(target);
    const Sphere impactorSphere = getBoundingSphere(impactor);

    // move target to origin
    displace(target, -targetSphere.center());

    // move impactor to impact angle
    const Float impactorDistance = targetSphere.radius() + impactorSphere.radius();

    const Float h = target.getValue<Vector>(QuantityId::POSITION)[0][H];
    const Float phi = geometry.get<Float>(CollisionGeometrySettingsId::IMPACT_ANGLE) * DEG_TO_RAD;
    SPH_ASSERT(phi >= -PI && phi <= PI, phi);

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
        moveToCenterOfMassSystem(target);
    }

    // merge bodies to single storage
    result = makeShared<ParticleData>();
    result->storage = std::move(target);
}

static JobRegistrar sRegisterCollisionSetup(
    "collision setup",
    "setup",
    "particle operators",
    [](const std::string& name) { return makeAuto<CollisionGeometrySetup>(name); },
    "Adds two input particle states (bodies) into a single state, moving the second body (impactor) to a "
    "position specified by the impact angle and adding an impact velocity to the impactor.");


// ----------------------------------------------------------------------------------------------------------
// SmoothedToSolidHandoff
// ----------------------------------------------------------------------------------------------------------

static RegisterEnum<HandoffRadius> sHandoffRadius({
    { HandoffRadius::EQUAL_VOLUME,
        "equal_volume",
        "Assume equal volume for solid spheres; r_solid = m / (4/3 pi rho_sph)^(1/3)." },
    { HandoffRadius::SMOOTHING_LENGTH,
        "smoothing_length",
        "Use a multiple of the smoothing length; r_solid = multiplier * h." },
});

VirtualSettings SmoothedToSolidHandoff::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& category = connector.addCategory("Handoff options");
    category.connect("Radius", "radius", type)
        .setTooltip("Determines how to compute the radii of the solid spheres. Can be one of:\n" +
                    EnumMap::getDesc<HandoffRadius>());
    category.connect("Radius multiplier", "radiusMultiplier", radiusMultiplier).setEnabler([this] {
        return HandoffRadius(type) == HandoffRadius::SMOOTHING_LENGTH;
    });

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
    SPH_ASSERT(r_sphere.size() == rho.size());
    for (Size i = 0; i < r_sphere.size(); ++i) {
        switch (HandoffRadius(type)) {
        case (HandoffRadius::EQUAL_VOLUME):
            r_sphere[i][H] = cbrt(3._f * m[i] / (4._f * PI * rho[i]));
            break;
        case (HandoffRadius::SMOOTHING_LENGTH):
            r_sphere[i][H] = radiusMultiplier * r_sphere[i][H];
            break;
        default:
            NOT_IMPLEMENTED;
        }
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

    moveToCenterOfMassSystem(spheres);

    result = makeShared<ParticleData>();
    result->storage = std::move(spheres);
}

static JobRegistrar sRegisterHandoff(
    "smoothed-to-solid handoff",
    "handoff",
    "particle operators",
    [](const std::string& name) { return makeAuto<SmoothedToSolidHandoff>(name); },
    "Converts smoothed particles, an output of SPH simulaion, into hard spheres that can be hand off to the "
    "N-body simulation.");


// ----------------------------------------------------------------------------------------------------------
// ExtractComponentJob
// ----------------------------------------------------------------------------------------------------------

VirtualSettings ExtractComponentJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& category = connector.addCategory("Component");
    category.connect("Component index", "index", componentIdx);
    category.connect("Connectivity factor", "factor", factor);
    category.connect("Move to CoM", "center", center);
    return connector;
}

void ExtractComponentJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
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
        moveToCenterOfMassSystem(storage);
    }

    result = makeShared<ParticleData>();
    result->storage = std::move(storage);
}

static JobRegistrar sRegisterExtractComponent(
    "extract component",
    "extractor",
    "particle operators",
    [](const std::string& name) { return makeAuto<ExtractComponentJob>(name); },
    "Preserves all particles belonging to the largest body in the input particle state (or optionally the "
    "n-th largest body) and removes all other particles. This modifier is useful to separate the largest "
    "remnant or the largest fragment in the result of a simulation.");

// ----------------------------------------------------------------------------------------------------------
// RemoveParticleJob
// ----------------------------------------------------------------------------------------------------------

VirtualSettings RemoveParticlesJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& category = connector.addCategory("Removal");
    category.connect("Remove damaged", "damaged.use", removeDamaged);
    category.connect("Damage limit", "damaged.limit", damageLimit).setEnabler([&] { //
        return removeDamaged;
    });
    category.connect("Remove expanded", "expanded.use", removeExpanded);
    category.connect("Energy limit", "expanded.limit", energyLimit).setEnabler([&] {
        return removeExpanded;
    });
    return connector;
}

void RemoveParticlesJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    Storage storage = std::move(this->getInput<ParticleData>("particles")->storage);
    std::set<Size> removeSet;
    if (removeDamaged) {
        ArrayView<const Float> d = storage.getValue<Float>(QuantityId::DAMAGE);
        for (Size i = 0; i < d.size(); ++i) {
            if (d[i] >= damageLimit) {
                removeSet.insert(i);
            }
        }
    }
    if (removeExpanded) {
        ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);
        for (Size i = 0; i < u.size(); ++i) {
            if (u[i] >= energyLimit) {
                removeSet.insert(i);
            }
        }
    }
    Array<Size> toRemove(removeSet.size());
    std::copy(removeSet.begin(), removeSet.end(), toRemove.begin());
    storage.remove(toRemove, Storage::IndicesFlag::INDICES_SORTED);

    result = makeShared<ParticleData>();
    result->storage = std::move(storage);
}

static JobRegistrar sRemoveParticles(
    "remove particles",
    "remover",
    "particle operators",
    [](const std::string& name) { return makeAuto<RemoveParticlesJob>(name); },
    "Removes all particles matching given conditions.");


// ----------------------------------------------------------------------------------------------------------
// MergeComponentsJob
// ----------------------------------------------------------------------------------------------------------

static RegisterEnum<ConnectivityEnum> sConnectivity({
    { ConnectivityEnum::OVERLAP, "overlap", "Overlap" },
    { ConnectivityEnum::ESCAPE_VELOCITY, "escape velocity", "Escape velocity" },
});

VirtualSettings MergeComponentsJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& category = connector.addCategory("Component");
    category.connect("Connectivity factor", "factor", factor);
    category.connect("Component definition", "definition", connectivity);
    return connector;
}

void MergeComponentsJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<ParticleData> particles = this->getInput<ParticleData>("particles");
    Storage& input = particles->storage;

    // allow using this for storage without masses --> add ad hoc mass if it's missing
    if (!input.has(QuantityId::MASS)) {
        input.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 1._f);
    }

    Array<Size> components;
    Flags<Post::ComponentFlag> flags = Post::ComponentFlag::OVERLAP;
    if (ConnectivityEnum(connectivity) == ConnectivityEnum::ESCAPE_VELOCITY) {
        flags = Post::ComponentFlag::ESCAPE_VELOCITY;
    }
    const Size componentCount = Post::findComponents(input, factor, flags, components);

    ArrayView<const Float> m = input.getValue<Float>(QuantityId::MASS);
    ArrayView<const Vector> r = input.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Vector> v = input.getDt<Vector>(QuantityId::POSITION);

    Array<Float> mc(componentCount);
    Array<Vector> rc(componentCount), vc(componentCount);
    Array<Float> hc(componentCount);

    mc.fill(0._f);
    rc.fill(Vector(0._f));
    vc.fill(Vector(0._f));
    hc.fill(0._f);

    for (Size i = 0; i < m.size(); ++i) {
        const Size ci = components[i];
        mc[ci] += m[i];
        rc[ci] += m[i] * r[i];
        vc[ci] += m[i] * v[i];
        hc[ci] += pow<3>(r[i][H]);
    }

    for (Size ci = 0; ci < componentCount; ++ci) {
        rc[ci] /= mc[ci];
        vc[ci] /= mc[ci];
        rc[ci][H] = cbrt(hc[ci]);
        vc[ci][H] = 0._f;
    }

    Storage output;
    output.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, std::move(mc));
    output
        .insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(rc)) //
        .getDt<Vector>() = std::move(vc);

    result = particles;
    result->storage = std::move(output);
}

static JobRegistrar sRegisterMergeComponents(
    "merge components",
    "merger",
    "particle operators",
    [](const std::string& name) { return makeAuto<MergeComponentsJob>(name); },
    "Merges all overlapping particles into larger spheres, preserving the total mass and volume of "
    "particles. Other quantities are handled as intensive, i.e. they are computed using weighted average.");

// ----------------------------------------------------------------------------------------------------------
// ExtractParticlesInDomainJob
// ----------------------------------------------------------------------------------------------------------

VirtualSettings ExtractParticlesInDomainJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& category = connector.addCategory("Misc");
    category.connect("Move to CoM", "center", center);
    return connector;
}

void ExtractParticlesInDomainJob::evaluate(const RunSettings& UNUSED(global),
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
        moveToCenterOfMassSystem(storage);
    }

    result = data;
}

static JobRegistrar sRegisterExtractInDomain(
    "extract particles in domain",
    "extractor",
    "particle operators",
    [](const std::string& name) { return makeAuto<ExtractParticlesInDomainJob>(name); },
    "Preserves only particles inside the given shape, particles outside the shape are removed.");

// ----------------------------------------------------------------------------------------------------------
// EmplaceComponentsAsFlagsJob
// ----------------------------------------------------------------------------------------------------------

VirtualSettings EmplaceComponentsAsFlagsJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& category = connector.addCategory("Component");
    category.connect("Connectivity factor", "factor", factor);
    return connector;
}

void EmplaceComponentsAsFlagsJob::evaluate(const RunSettings& UNUSED(global),
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

static JobRegistrar sRegisterEmplaceComponents(
    "emplace components",
    "emplacer",
    "particle operators",
    [](const std::string& name) { return makeAuto<EmplaceComponentsAsFlagsJob>(name); },
    "This modifier detects components (i.e. separated bodies) in the \"fragments\" particle input and stores "
    "the indices of the components as flags to the other particle input \"original\". This is useful to "
    "visualize the particles belonging to different fragments in the initial conditions of the simulation.");


// ----------------------------------------------------------------------------------------------------------
// SubsampleJob
// ----------------------------------------------------------------------------------------------------------

VirtualSettings SubsampleJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& category = connector.addCategory("Subsampling");
    category.connect("Fraction", "fraction", fraction);
    return connector;
}

void SubsampleJob::evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) {
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

static JobRegistrar sRegisterSubsampler(
    "subsampler",
    "particle operators",
    [](const std::string& name) { return makeAuto<SubsampleJob>(name); },
    "Preserves a fraction of randomly selected particles, removes the other particles.");


// ----------------------------------------------------------------------------------------------------------
// CompareJob
// ----------------------------------------------------------------------------------------------------------

static RegisterEnum<CompareMode> sCompare({
    { CompareMode::PARTICLE_WISE,
        "particle_wise",
        "States must have the same number of particles. Compares all quantities of particles at "
        "corresponding indices. Viable for SPH simulations." },
    { CompareMode::LARGE_PARTICLES_ONLY,
        "large_particles_only",
        "Compares only large particles in the states. The number of particles may be different and the "
        "indices of particles do not have to match. Viable for N-body simulations with merging." },
});


VirtualSettings CompareJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    auto nbodyEnabler = [&] { return CompareMode(mode) == CompareMode::LARGE_PARTICLES_ONLY; };

    VirtualSettings::Category& compareCat = connector.addCategory("Comparison");
    compareCat.connect("Compare mode", "compare_mode", mode);
    compareCat.connect("Tolerance", "eps", eps);
    compareCat.connect("Fraction", "fraction", fraction).setEnabler(nbodyEnabler);
    compareCat.connect("Max deviation [km]", "max_deviation", maxDeviation)
        .setUnits(1.e3_f)
        .setEnabler(nbodyEnabler);

    return connector;
}

void CompareJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    Storage& test = this->getInput<ParticleData>("test particles")->storage;
    Storage& ref = this->getInput<ParticleData>("reference particles")->storage;

    Outcome result = SUCCESS;
    switch (CompareMode(mode)) {
    case CompareMode::PARTICLE_WISE:
        result = Post::compareParticles(test, ref, eps);
        break;
    case CompareMode::LARGE_PARTICLES_ONLY:
        result = Post::compareLargeSpheres(test, ref, fraction, maxDeviation, eps);
        break;
    default:
        NOT_IMPLEMENTED;
    }
    if (!result) {
        throw InvalidSetup(result.error());
    }
}

static JobRegistrar sRegisterCompare(
    "compare",
    "particle operators",
    [](const std::string& name) { return makeAuto<CompareJob>(name); },
    "Compares two states. If a difference is found, it is shown as an error dialog.");


NAMESPACE_SPH_END
