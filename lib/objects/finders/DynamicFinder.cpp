#include "objects/finders/DynamicFinder.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/Voxel.h"
#include "objects/geometry/SymmetricTensor.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

DynamicFinder::DynamicFinder(const RunSettings& settings) {
    compactThreshold = settings.get<Float>(RunSettingsId::SPH_FINDER_COMPACT_THRESHOLD);
}

void DynamicFinder::buildImpl(ArrayView<const Vector> points) {
    this->updateFinder(points);
    actual->buildImpl(points);
}

void DynamicFinder::rebuildImpl(ArrayView<const Vector> points) {
    this->updateFinder(points);
    actual->rebuildImpl(points);
}

Size DynamicFinder::findNeighbours(const Size index,
    const Float radius,
    Array<NeighbourRecord>& neighbours,
    Flags<FinderFlags> flags,
    const Float error) const {
    return actual->findNeighbours(index, radius, neighbours, flags, error);
}

Size DynamicFinder::findNeighbours(const Vector& position,
    const Float radius,
    Array<NeighbourRecord>& neighbours,
    Flags<FinderFlags> flags,
    const Float error) const {
    return actual->findNeighbours(position, radius, neighbours, flags, error);
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
        if (!actual || typeid(*actual) != typeid(VoxelFinder)) {
            actual = makeAuto<VoxelFinder>();
        }
    } else {
        if (!actual || typeid(*actual) != typeid(KdTree)) {
            actual = makeAuto<KdTree>();
        }
    }
    ASSERT(actual != nullptr);
    return metric;
}

NAMESPACE_SPH_END
