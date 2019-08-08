#include "run/workers/GeometryWorkers.h"
#include "math/rng/VectorRng.h"
#include "objects/geometry/Sphere.h"
#include "post/MarchingCubes.h"
#include "post/MeshFile.h"
#include "run/IRun.h"
#include "sph/initial/MeshDomain.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

//-----------------------------------------------------------------------------------------------------------
// SphereWorker
//-----------------------------------------------------------------------------------------------------------

SphereWorker::SphereWorker(const std::string& name)
    : IGeometryWorker(name) {}

std::string SphereWorker::className() const {
    return "sphere";
}

UnorderedMap<std::string, WorkerType> SphereWorker::getSlots() const {
    return {};
}

VirtualSettings SphereWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& geoCat = connector.addCategory("geometry");
    geoCat.connect("radius [km]", "radius", radius, 1.e3_f);
    return connector;
}

void SphereWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = makeAuto<SphericalDomain>(Vector(0._f), radius);
}

static WorkerRegistrar sRegisterSphere("sphere", "geometry", [](const std::string& name) {
    return makeAuto<SphereWorker>(name);
});

//-----------------------------------------------------------------------------------------------------------
// BlockWorker
//-----------------------------------------------------------------------------------------------------------

VirtualSettings BlockWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& geoCat = connector.addCategory("geometry");
    geoCat.connect("center [km]", "center", center, 1.e3_f);
    geoCat.connect("dimensions [km]", "dimensions", dimensions, 1.e3_f);
    return connector;
}

void BlockWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = makeAuto<BlockDomain>(center, dimensions);
}

static WorkerRegistrar sRegisterBlock("block", "geometry", [](const std::string& name) {
    return makeAuto<BlockWorker>(name);
});

//-----------------------------------------------------------------------------------------------------------
// EllipsoidWorker
//-----------------------------------------------------------------------------------------------------------

VirtualSettings EllipsoidWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& geoCat = connector.addCategory("geometry");
    geoCat.connect("semi-axes [km]", "semixes", semiaxes, 1.e3_f);
    return connector;
}

void EllipsoidWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = makeAuto<EllipsoidalDomain>(Vector(0._f), semiaxes);
}

static WorkerRegistrar sRegisterEllipsoid("ellipsoid", "geometry", [](const std::string& name) {
    return makeAuto<EllipsoidWorker>(name);
});

//-----------------------------------------------------------------------------------------------------------
// CylinderWorker
//-----------------------------------------------------------------------------------------------------------

CylinderWorker::CylinderWorker(const std::string& name)
    : IGeometryWorker(name) {}

std::string CylinderWorker::className() const {
    return "cylinder";
}

UnorderedMap<std::string, WorkerType> CylinderWorker::getSlots() const {
    return {};
}

VirtualSettings CylinderWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& geoCat = connector.addCategory("geometry");
    geoCat.connect("height [km]", "height", height, 1.e3_f).connect("radius [km]", "radius", radius, 1.e3_f);
    return connector;
}

void CylinderWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = makeAuto<CylindricalDomain>(Vector(0._f), radius, height, true);
}

static WorkerRegistrar sRegisterCylinder("cylinder", "geometry", [](const std::string& name) {
    return makeAuto<CylinderWorker>(name);
});

//-----------------------------------------------------------------------------------------------------------
// MeshGeometryWorker
//-----------------------------------------------------------------------------------------------------------

MeshGeometryWorker::MeshGeometryWorker(const std::string& name)
    : IGeometryWorker(name) {}

std::string MeshGeometryWorker::className() const {
    return "triangle mesh";
}

UnorderedMap<std::string, WorkerType> MeshGeometryWorker::getSlots() const {
    return {};
}

VirtualSettings MeshGeometryWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& pathCat = connector.addCategory("Mesh source");
    pathCat.connect("Path", "path", path).connect("Scaling factor", "scale", scale);
    return connector;
}

void MeshGeometryWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    AutoPtr<IMeshFile> meshLoader = getMeshFile(path);
    Expected<Array<Triangle>> triangles = meshLoader->load(path);
    if (!triangles) {
        throw InvalidSetup("cannot load " + path.native());
    }
    result = makeAuto<MeshDomain>(std::move(triangles.value()), AffineMatrix::scale(Vector(scale)));
}

static WorkerRegistrar sRegisterMeshGeometry("triangle mesh",
    "mesh",
    "geometry",
    [](const std::string& name) { return makeAuto<MeshGeometryWorker>(name); });

//-----------------------------------------------------------------------------------------------------------
// ParticleGeometryWorker
//-----------------------------------------------------------------------------------------------------------

VirtualSettings ParticleGeometryWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& pathCat = connector.addCategory("Surface");
    pathCat.connect("Spatial resolution [m]", "resolution", resolution)
        .connect("Iso-surface value", "level", surfaceLevel);
    return connector;
}

void ParticleGeometryWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& callbacks) {
    Storage input = std::move(this->getInput<ParticleData>("particles")->storage);
    // sanitize the resolution
    ArrayView<const Vector> r = input.getValue<Vector>(QuantityId::POSITION);
    Box boundingBox;
    for (Size i = 0; i < r.size(); ++i) {
        boundingBox.extend(r[i]);
    }
    const Float scale = maxElement(boundingBox.size());
    const Float actResolution = clamp(resolution, 0.001_f * scale, 0.25_f * scale);

    SharedPtr<IScheduler> scheduler = Factory::getScheduler(RunSettings::getDefaults());

    auto callback = [&callbacks](const Float progress) {
        Statistics stats;
        stats.set(StatisticsId::RELATIVE_PROGRESS, progress);
        callbacks.onTimeStep(Storage(), stats);
        return !callbacks.shouldAbortRun();
    };
    Array<Triangle> triangles = getSurfaceMesh(*scheduler, input, actResolution, surfaceLevel, callback);
    result = makeAuto<MeshDomain>(std::move(triangles));
}

static WorkerRegistrar sRegisterParticleGeometry("particle geometry",
    "particles",
    "geometry",
    [](const std::string& name) { return makeAuto<ParticleGeometryWorker>(name); });


//-----------------------------------------------------------------------------------------------------------
// SpheresGeometryWorker
//-----------------------------------------------------------------------------------------------------------

class SpheresDomain : public IDomain {
private:
    Array<Sphere> spheres;
    Box boundingBox;

public:
    explicit SpheresDomain(ArrayView<const Vector> r) {
        spheres.resize(r.size());
        for (Size i = 0; i < r.size(); ++i) {
            spheres[i] = Sphere(r[i], r[i][H]);
            boundingBox.extend(r[i] + Vector(r[i][H]));
            boundingBox.extend(r[i] - Vector(r[i][H]));
        }
    }

    virtual Vector getCenter() const override {
        return boundingBox.center();
    }

    virtual Box getBoundingBox() const override {
        return boundingBox;
    }

    virtual Float getVolume() const override {
        Float volume = 0._f;
        for (const Sphere& s : spheres) {
            volume += s.volume();
        }
        return volume;
    }

    virtual bool contains(const Vector& v) const override {
        if (!boundingBox.contains(v)) {
            return false;
        }
        for (const Sphere& s : spheres) {
            if (s.contains(v)) {
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
};

VirtualSettings SpheresGeometryWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    return connector;
}

void SpheresGeometryWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<ParticleData> data = this->getInput<ParticleData>("spheres");
    ArrayView<const Vector> r = data->storage.getValue<Vector>(QuantityId::POSITION);
    result = makeShared<SpheresDomain>(r);
}

static WorkerRegistrar sRegisterSpheresGeometry("spheres geometry",
    "spheres",
    "geometry",
    [](const std::string& name) { return makeAuto<SpheresGeometryWorker>(name); });

//-----------------------------------------------------------------------------------------------------------
// InvertGeometryWorker
//-----------------------------------------------------------------------------------------------------------

class InvertDomain : public IDomain {
private:
    SharedPtr<IDomain> domain;

public:
    explicit InvertDomain(SharedPtr<IDomain> domain)
        : domain(domain) {}

    virtual Vector getCenter() const override {
        return domain->getCenter();
    }

    virtual Box getBoundingBox() const override {
        return Box(Vector(-LARGE), Vector(LARGE));
    }

    virtual Float getVolume() const override {
        return LARGE;
    }

    virtual bool contains(const Vector& v) const override {
        return !domain->contains(v);
    }

    virtual void getSubset(ArrayView<const Vector> vs,
        Array<Size>& output,
        const SubsetType type) const override {
        const SubsetType invertedType =
            (type == SubsetType::INSIDE) ? SubsetType::OUTSIDE : SubsetType::INSIDE;
        return domain->getSubset(vs, output, invertedType);
    }

    virtual void getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const override {
        domain->getDistanceToBoundary(vs, distances);
        for (Float& dist : distances) {
            dist *= -1._f;
        }
    }

    virtual void project(ArrayView<Vector>, Optional<ArrayView<Size>>) const override {
        NOT_IMPLEMENTED;
    }

    virtual void addGhosts(ArrayView<const Vector>, Array<Ghost>&, const Float, const Float) const override {
        NOT_IMPLEMENTED;
    }
};

VirtualSettings InvertGeometryWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    return connector;
}

void InvertGeometryWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<IDomain> domain = this->getInput<IDomain>("geometry");
    result = makeShared<InvertDomain>(domain);
}

static WorkerRegistrar sRegisterInvertGeometry("invert geometry",
    "inverter",
    "geometry",
    [](const std::string& name) { return makeAuto<InvertGeometryWorker>(name); });

//-----------------------------------------------------------------------------------------------------------
// TransformGeometryWorker
//-----------------------------------------------------------------------------------------------------------


VirtualSettings TransformGeometryWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& transformCat = connector.addCategory("Transform");
    transformCat.connect("Scaling", "scaling", scaling).connect("Offset", "offset", offset);
    return connector;
}

void TransformGeometryWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<IDomain> domain = this->getInput<IDomain>("geometry");
    const Vector center = domain->getCenter();
    AffineMatrix matrix = AffineMatrix::identity();
    matrix.translate(-center);
    matrix = AffineMatrix::scale(scaling) * matrix;
    matrix.translate(center + offset);
    result = makeShared<TransformedDomain>(domain, matrix);
}

static WorkerRegistrar sRegisterTransformGeometry("transform geometry",
    "transform",
    "geometry",
    [](const std::string& name) { return makeAuto<TransformGeometryWorker>(name); });

//-----------------------------------------------------------------------------------------------------------
// BooleanGeometryWorker
//-----------------------------------------------------------------------------------------------------------

class BooleanDomain : public IDomain {
private:
    SharedPtr<IDomain> operA;
    SharedPtr<IDomain> operB;
    Vector offset;
    BooleanEnum mode;

    Float volume;

public:
    BooleanDomain(SharedPtr<IDomain> operA,
        SharedPtr<IDomain> operB,
        const Vector& offset,
        const BooleanEnum mode)
        : operA(operA)
        , operB(operB)
        , offset(offset)
        , mode(mode) {
        // avoid integration for invalid bbox
        const Box box = this->getBoundingBox();
        if (box == Box::EMPTY()) {
            volume = 0._f;
        } else {
            const Size N = 100000;
            Size inside = 0;
            VectorRng<UniformRng> rng;
            for (Size i = 0; i < N; ++i) {
                const Vector r = box.lower() + rng() * box.size();
                inside += int(this->contains(r));
            }
            volume = box.volume() * Float(inside) / N;
        }

        if (volume == 0._f) {
            throw InvalidSetup("The boolean domain is empty.");
        }
    }

    virtual Vector getCenter() const override {
        return this->getBoundingBox().center();
    }

    virtual Box getBoundingBox() const override {
        Box boxA = operA->getBoundingBox();
        Box boxB = operB->getBoundingBox().translate(offset);
        switch (mode) {
        case BooleanEnum::UNION:
            boxA.extend(boxB);
            break;
        case BooleanEnum::INTERSECTION:
            boxA = boxA.intersect(boxB);
            break;
        case BooleanEnum::DIFFERENCE:
            break;
        default:
            NOT_IMPLEMENTED;
        }
        return boxA;
    }

    virtual Float getVolume() const override {
        return volume;
    }

    virtual bool contains(const Vector& v1) const override {
        const Vector v2 = v1 - offset;
        switch (mode) {
        case BooleanEnum::UNION:
            return operA->contains(v1) || operB->contains(v2);
        case BooleanEnum::INTERSECTION:
            return operA->contains(v1) && operB->contains(v2);
        case BooleanEnum::DIFFERENCE:
            return operA->contains(v1) && !operB->contains(v2);
        default:
            NOT_IMPLEMENTED;
        }
    }

    virtual void getSubset(ArrayView<const Vector> vs,
        Array<Size>& output,
        const SubsetType type) const override {
        switch (type) {
        case SubsetType::OUTSIDE:
            for (Size i = 0; i < vs.size(); ++i) {
                if (!this->contains(vs[i])) {
                    output.push(i);
                }
            }
            break;
        case SubsetType::INSIDE:
            for (Size i = 0; i < vs.size(); ++i) {
                if (this->contains(vs[i])) {
                    output.push(i);
                }
            }
        }
    }

    virtual void getDistanceToBoundary(ArrayView<const Vector>, Array<Float>&) const override {
        NOT_IMPLEMENTED
    }

    virtual void project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices) const override {
        if (mode != BooleanEnum::UNION) {
            NOT_IMPLEMENTED;
        }

        /// \todo this is not correct, do properly
        operA->project(vs, indices);
        operB->project(vs, indices);
    }

    virtual void addGhosts(ArrayView<const Vector> vs,
        Array<Ghost>& ghosts,
        const Float eta,
        const Float eps) const override {
        if (mode != BooleanEnum::UNION) {
            NOT_IMPLEMENTED;
        }

        /// \todo this is not correct, do properly
        operA->addGhosts(vs, ghosts, eta, eps);
        operB->addGhosts(vs, ghosts, eta, eps);
    }
};

void BooleanGeometryWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<IDomain> operA = this->getInput<IDomain>("operand A");
    SharedPtr<IDomain> operB = this->getInput<IDomain>("operand B");
    result = makeAuto<BooleanDomain>(operA, operB, offset, BooleanEnum(mode.value));
}

VirtualSettings BooleanGeometryWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& boolCat = connector.addCategory("Boolean");
    boolCat.connect("Operation", "operation", mode).connect("Offset [km]", "offset", offset, 1.e3_f);

    return connector;
}

static WorkerRegistrar sRegisterBoolean("boolean", "geometry", [](const std::string& name) {
    return makeAuto<BooleanGeometryWorker>(name);
});


NAMESPACE_SPH_END
