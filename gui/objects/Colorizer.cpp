#include "gui/objects/Colorizer.h"

NAMESPACE_SPH_BEGIN

DirectionColorizer::DirectionColorizer(const Vector& axis, const Palette& palette)
    : palette(palette)
    , axis(axis) {
    SPH_ASSERT(almostEqual(getLength(axis), 1._f));
    // compute 2 perpendicular directions
    Vector ref;
    if (almostEqual(axis, Vector(0._f, 0._f, 1._f)) || almostEqual(axis, Vector(0._f, 0._f, -1._f))) {
        ref = Vector(0._f, 1._f, 0._f);
    } else {
        ref = Vector(0._f, 0._f, 1._f);
    }
    dir1 = getNormalized(cross(axis, ref));
    dir2 = cross(axis, dir1);
    SPH_ASSERT(almostEqual(getLength(dir2), 1._f));
}

Optional<float> DirectionColorizer::evalScalar(const Size idx) const {
    SPH_ASSERT(this->isInitialized());
    const Vector projected = values[idx] - dot(values[idx], axis) * axis;
    const Float x = dot(projected, dir1);
    const Float y = dot(projected - x * dir1, dir2);
    return float(PI + atan2(y, x));
}

static thread_local Array<NeighbourRecord> neighs;

SummedDensityColorizer::SummedDensityColorizer(const RunSettings& settings, Palette palette)
    : palette(std::move(palette)) {
    finder = Factory::getFinder(settings);
    kernel = Factory::getKernel<3>(settings);
}

void SummedDensityColorizer::initialize(const Storage& storage, const RefEnum ref) {
    m = makeArrayRef(storage.getValue<Float>(QuantityId::MASS), ref);
    r = makeArrayRef(storage.getValue<Vector>(QuantityId::POSITION), ref);

    finder->build(SEQUENTIAL, r);
}

float SummedDensityColorizer::sum(const Size idx) const {
    finder->findAll(idx, r[idx][H] * kernel.radius(), neighs);
    Float rho = 0._f;
    for (const auto& n : neighs) {
        rho += m[n.index] * kernel.value(r[idx] - r[n.index], r[idx][H]);
    }
    return float(rho);
}

void CorotatingVelocityColorizer::initialize(const Storage& storage, const RefEnum ref) {
    r = makeArrayRef(storage.getValue<Vector>(QuantityId::POSITION), ref);
    v = makeArrayRef(storage.getDt<Vector>(QuantityId::POSITION), ref);
    matIds = makeArrayRef(storage.getValue<Size>(QuantityId::MATERIAL_ID), ref);

    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Vector> rv = r;
    ArrayView<const Vector> vv = v;
    data.resize(storage.getMaterialCnt());

    for (Size i = 0; i < data.size(); ++i) {
        MaterialView mat = storage.getMaterial(i);
        const Size from = *mat.sequence().begin();
        const Size to = *mat.sequence().end();
        const Size size = to - from;
        data[i].center = Post::getCenterOfMass(m.subset(from, size), rv.subset(from, size));
        data[i].omega =
            Post::getAngularFrequency(m.subset(from, size), rv.subset(from, size), vv.subset(from, size));
    }
}

NAMESPACE_SPH_END
