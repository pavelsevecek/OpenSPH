#include "objects/finders/DynamicFinder.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/UniformGrid.h"
#include "objects/geometry/SymmetricTensor.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

DynamicFinder::DynamicFinder(const RunSettings& settings) {
    compactThreshold = settings.get<Float>(RunSettingsId::SPH_FINDER_COMPACT_THRESHOLD);
}

void DynamicFinder::buildImpl(IScheduler& scheduler, ArrayView<const Vector> points) {
    this->updateFinder(points);
    actual->buildImpl(scheduler, points);
}

Size DynamicFinder::findAll(const Size index, const Float radius, Array<NeighbourRecord>& neighbours) const {
    return actual->findAll(index, radius, neighbours);
}

Size DynamicFinder::findAll(const Vector& position,
    const Float radius,
    Array<NeighbourRecord>& neighbours) const {
    return actual->findAll(position, radius, neighbours);
}

Size DynamicFinder::findLowerRank(const Size index,
    const Float radius,
    Array<NeighbourRecord>& neighbours) const {
    return actual->findLowerRank(index, radius, neighbours);
}

Float DynamicFinder::updateFinder(ArrayView<const Vector> points) {
    // compute dipole and quadrupole moments (assuming all particles have the same mass)
    Vector dipole(0._f);
    SymmetricTensor quadrupole(0._f);
    Box box;

    for (Vector p : points) {
        box.extend(p);
        dipole += p;
        quadrupole += outer(p, p);
    }

    // use PAT on tensors
    dipole -= points.size() * box.center();
    quadrupole -= points.size() * outer(box.center(), box.center());

    // compute final metric using empirical expression (no science here)
    const Float size = getLength(box.size()) * points.size();
    const Float metric = getLength(dipole) / size + norm(quadrupole) / sqr(size);
    ASSERT(metric >= 0._f && metric <= 2._f);

    // choose the finder implementation based on the metric value
    if (metric <= compactThreshold) {
        // particles seem to be compact enough, use voxel finder
        if (!actual || dynamic_cast<UniformGridFinder*>(&*actual)) {
            actual = makeAuto<UniformGridFinder>();
        }
    } else {
        if (!actual || dynamic_cast<KdTree<KdNode>*>(&*actual)) {
            actual = makeAuto<KdTree<KdNode>>();
        }
    }
    ASSERT(actual != nullptr);
    return metric;
}

NAMESPACE_SPH_END
