#include "run/workers/GeometryJobs.h"
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

SphereJob::SphereJob(const std::string& name)
    : IGeometryJob(name) {}

std::string SphereJob::className() const {
    return "sphere";
}

UnorderedMap<std::string, JobType> SphereJob::getSlots() const {
    return {};
}

VirtualSettings SphereJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& geoCat = connector.addCategory("geometry");
    geoCat.connect("radius [km]", "radius", radius).setUnits(1.e3_f);
    return connector;
}

void SphereJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = makeAuto<SphericalDomain>(Vector(0._f), radius);
}

static JobRegistrar sRegisterSphere(
    "sphere",
    "geometry",
    [](const std::string& name) { return makeAuto<SphereJob>(name); },
    "Geometric shape representing a sphere with given radius.");

//-----------------------------------------------------------------------------------------------------------
// BlockWorker
//-----------------------------------------------------------------------------------------------------------

VirtualSettings BlockJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& geoCat = connector.addCategory("geometry");
    geoCat.connect("center [km]", "center", center).setUnits(1.e3_f);
    geoCat.connect("dimensions [km]", "dimensions", dimensions).setUnits(1.e3_f);
    return connector;
}

void BlockJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = makeAuto<BlockDomain>(center, dimensions);
}

static JobRegistrar sRegisterBlock(
    "block",
    "geometry",
    [](const std::string& name) { return makeAuto<BlockJob>(name); },
    "Geometric shape representing a block with given dimensions.");

//-----------------------------------------------------------------------------------------------------------
// EllipsoidWorker
//-----------------------------------------------------------------------------------------------------------

VirtualSettings EllipsoidJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& geoCat = connector.addCategory("geometry");
    geoCat.connect("semi-axes [km]", "semixes", semiaxes).setUnits(1.e3_f);
    return connector;
}

void EllipsoidJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = makeAuto<EllipsoidalDomain>(Vector(0._f), semiaxes);
}

static JobRegistrar sRegisterEllipsoid(
    "ellipsoid",
    "geometry",
    [](const std::string& name) { return makeAuto<EllipsoidJob>(name); },
    "Geometric shape representing a triaxial ellipsoid.");

//-----------------------------------------------------------------------------------------------------------
// CylinderWorker
//-----------------------------------------------------------------------------------------------------------

CylinderJob::CylinderJob(const std::string& name)
    : IGeometryJob(name) {}

std::string CylinderJob::className() const {
    return "cylinder";
}

UnorderedMap<std::string, JobType> CylinderJob::getSlots() const {
    return {};
}

VirtualSettings CylinderJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& geoCat = connector.addCategory("geometry");
    geoCat.connect("height [km]", "height", height).setUnits(1.e3_f);
    geoCat.connect("radius [km]", "radius", radius).setUnits(1.e3_f);
    return connector;
}

void CylinderJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = makeAuto<CylindricalDomain>(Vector(0._f), radius, height, true);
}

static JobRegistrar sRegisterCylinder(
    "cylinder",
    "geometry",
    [](const std::string& name) { return makeAuto<CylinderJob>(name); },
    "Geometric shape representing a cylinder aligned with z-axis, using provided radius and height.");

//-----------------------------------------------------------------------------------------------------------
// HalfSpaceWorker
//-----------------------------------------------------------------------------------------------------------

VirtualSettings HalfSpaceJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    return connector;
}

void HalfSpaceJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = makeAuto<HalfSpaceDomain>();
}

static JobRegistrar sRegisterHalfSpace(
    "half space",
    "geometry",
    [](const std::string& name) { return makeAuto<HalfSpaceJob>(name); },
    "Represents a half space z>0. Note that this cannot be used as a domain for generating particles as the "
    "volume of the domain is infinite. It can be used as an input to a composite domain (boolean, etc.) or "
    "as a domain for boundary conditions of a simulation.");


//-----------------------------------------------------------------------------------------------------------
// GaussianSphereWorker
//-----------------------------------------------------------------------------------------------------------

VirtualSettings GaussianSphereJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& geoCat = connector.addCategory("geometry");
    geoCat.connect("radius [km]", "radius", radius).setUnits(1.e3_f);
    geoCat.connect("variance", "variance", beta);
    geoCat.connect("random seed", "seed", seed);
    return connector;
}

void GaussianSphereJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    result = makeAuto<GaussianRandomSphere>(Vector(0._f), radius, beta, seed);
}

static JobRegistrar sRegisterGaussian(
    "Gaussian sphere",
    "geometry",
    [](const std::string& name) { return makeAuto<GaussianSphereJob>(name); },
    "TODO");

//-----------------------------------------------------------------------------------------------------------
// MeshGeometryWorker
//-----------------------------------------------------------------------------------------------------------

MeshGeometryJob::MeshGeometryJob(const std::string& name)
    : IGeometryJob(name) {}

std::string MeshGeometryJob::className() const {
    return "triangle mesh";
}

UnorderedMap<std::string, JobType> MeshGeometryJob::getSlots() const {
    return {};
}

VirtualSettings MeshGeometryJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& pathCat = connector.addCategory("Mesh source");
    pathCat.connect("Path", "path", path);
    pathCat.connect("Scaling factor", "scale", scale);
    pathCat.connect("Precompute", "precompute", precompute);
    return connector;
}

void MeshGeometryJob::evaluate(const RunSettings& global, IRunCallbacks& UNUSED(callbacks)) {
    AutoPtr<IMeshFile> meshLoader = getMeshFile(path);
    Expected<Array<Triangle>> triangles = meshLoader->load(path);
    if (!triangles) {
        throw InvalidSetup("cannot load " + path.native());
    }

    SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);
    MeshParams params;
    params.matrix = AffineMatrix::scale(Vector(scale));
    params.precomputeInside = precompute;
    result = makeAuto<MeshDomain>(*scheduler, std::move(triangles.value()), params);
}

static JobRegistrar sRegisterMeshGeometry(
    "triangle mesh",
    "mesh",
    "geometry",
    [](const std::string& name) { return makeAuto<MeshGeometryJob>(name); },
    "Geometric shape given by provided triangular mesh.");

//-----------------------------------------------------------------------------------------------------------
// ParticleGeometryWorker
//-----------------------------------------------------------------------------------------------------------

VirtualSettings ParticleGeometryJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    VirtualSettings::Category& pathCat = connector.addCategory("Surface");
    pathCat.connect("Spatial resolution [m]", "resolution", resolution);
    pathCat.connect("Iso-surface value", "level", surfaceLevel);
    return connector;
}

void ParticleGeometryJob::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    Storage input = std::move(this->getInput<ParticleData>("particles")->storage);
    // sanitize the resolution
    const Box boundingBox = getBoundingBox(input);
    const Float scale = maxElement(boundingBox.size());
    const Float actResolution = clamp(resolution, 0.001_f * scale, 0.25_f * scale);

    SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);

    auto callback = [&callbacks](const Float progress) {
        Statistics stats;
        stats.set(StatisticsId::RELATIVE_PROGRESS, progress);
        callbacks.onTimeStep(Storage(), stats);
        return !callbacks.shouldAbortRun();
    };
    Array<Triangle> triangles =
        getSurfaceMesh(*scheduler, input, actResolution, surfaceLevel, smoothingMult, callback);
    result = makeAuto<MeshDomain>(*scheduler, std::move(triangles));
}

static JobRegistrar sRegisterParticleGeometry(
    "particle geometry",
    "particles",
    "geometry",
    [](const std::string& name) { return makeAuto<ParticleGeometryJob>(name); },
    "Geometric shape represented by input particles");


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

    virtual Float getSurfaceArea() const override {
        Float area = 0._f;
        for (const Sphere& s : spheres) {
            area += sphereSurfaceArea(s.radius());
        }
        return area;
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

VirtualSettings SpheresGeometryJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    return connector;
}

void SpheresGeometryJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<ParticleData> data = this->getInput<ParticleData>("spheres");
    ArrayView<const Vector> r = data->storage.getValue<Vector>(QuantityId::POSITION);
    result = makeShared<SpheresDomain>(r);
}

static JobRegistrar sRegisterSpheresGeometry(
    "spheres geometry",
    "spheres",
    "geometry",
    [](const std::string& name) { return makeAuto<SpheresGeometryJob>(name); },
    "Geometric shape given by a set of spheres, specifies by the input particles.");

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

    virtual Float getSurfaceArea() const override {
        return domain->getSurfaceArea();
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

VirtualSettings InvertGeometryJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);
    return connector;
}

void InvertGeometryJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<IDomain> domain = this->getInput<IDomain>("geometry");
    result = makeShared<InvertDomain>(domain);
}

static JobRegistrar sRegisterInvertGeometry(
    "invert geometry",
    "inverter",
    "geometry",
    [](const std::string& name) { return makeAuto<InvertGeometryJob>(name); },
    "Shape modifier that inverts the geometry, i.e. swaps the outside and inside of a shape. This converts a "
    "sphere into a space with spherical hole, etc.");

//-----------------------------------------------------------------------------------------------------------
// TransformGeometryWorker
//-----------------------------------------------------------------------------------------------------------


VirtualSettings TransformGeometryJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& transformCat = connector.addCategory("Transform");
    transformCat.connect("Scaling", "scaling", scaling);
    transformCat.connect("Offset", "offset", offset);
    return connector;
}

void TransformGeometryJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<IDomain> domain = this->getInput<IDomain>("geometry");
    const Vector center = domain->getCenter();
    AffineMatrix matrix = AffineMatrix::identity();
    matrix.translate(-center);
    matrix = AffineMatrix::scale(scaling) * matrix;
    matrix.translate(center + offset);
    result = makeShared<TransformedDomain>(domain, matrix);
}

static JobRegistrar sRegisterTransformGeometry(
    "transform geometry",
    "transform",
    "geometry",
    [](const std::string& name) { return makeAuto<TransformGeometryJob>(name); },
    "Shape modifier, adding a translation and scaling to the input geometry.");

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

    virtual Float getSurfaceArea() const override {
        NOT_IMPLEMENTED;
        return 0._f;
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

void BooleanGeometryJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<IDomain> operA = this->getInput<IDomain>("operand A");
    SharedPtr<IDomain> operB = this->getInput<IDomain>("operand B");
    result = makeAuto<BooleanDomain>(operA, operB, offset, BooleanEnum(mode.value));
}

VirtualSettings BooleanGeometryJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& boolCat = connector.addCategory("Boolean");
    boolCat.connect("Operation", "operation", mode);
    boolCat.connect("Offset [km]", "offset", offset).setUnits(1.e3_f);

    return connector;
}

static JobRegistrar sRegisterBoolean(
    "boolean",
    "geometry",
    [](const std::string& name) { return makeAuto<BooleanGeometryJob>(name); },
    "Composite shape that applies given boolean operation to two input shapes.");


NAMESPACE_SPH_END
