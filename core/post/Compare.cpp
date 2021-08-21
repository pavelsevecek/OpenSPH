#include "post/Compare.h"
#include "objects/finders/KdTree.h"
#include "objects/finders/Order.h"
#include "quantities/Iterate.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

Outcome Post::compareParticles(const Storage& test, const Storage& ref, const Float eps) {
    if (test.getParticleCnt() != ref.getParticleCnt()) {
        return makeFailed("Different number of particles.\nTest has {}\nReference has {}",
            test.getParticleCnt(),
            ref.getParticleCnt());
    }

    if (test.getQuantityCnt() != ref.getQuantityCnt()) {
        return makeFailed("Different number of quantities.\nTest has {}\nReference has{}",
            test.getQuantityCnt(),
            ref.getQuantityCnt());
    }

    Outcome result = SUCCESS;
    auto checkZeroOrder = [&](QuantityId id, const auto& px, const auto& cx) {
        if (!result) {
            // mismatch already found
            return;
        }
        for (Size i = 0; i < px.size(); ++i) {
            if (!almostEqual(px[i], cx[i], eps)) {
                result =
                    makeFailed("Difference in {}\n{}=={}\n\n", getMetadata(id).quantityName, px[i], cx[i]);
                return;
            }
        }
    };
    iteratePair<VisitorEnum::ZERO_ORDER>(test, ref, checkZeroOrder);

    auto checkFirstOrder =
        [&](QuantityId id, const auto& px, const auto& pdx, const auto& cx, const auto& cdx) {
            if (!result) {
                return;
            }
            for (Size i = 0; i < px.size(); ++i) {
                if (!almostEqual(px[i], cx[i], eps)) {
                    result = makeFailed(
                        "Difference in {}\n{} == {}\n\n", getMetadata(id).quantityName, px[i], cx[i]);
                    return;
                }
                if (!almostEqual(pdx[i], cdx[i], eps)) {
                    result = makeFailed(
                        "Difference in {}\n{} == {}\n\n", getMetadata(id).derivativeName, pdx[i], cdx[i]);
                    return;
                }
            }
        };
    iteratePair<VisitorEnum::FIRST_ORDER>(test, ref, checkFirstOrder);

    auto checkSecondOrder = [&](QuantityId id,
                                const auto& px,
                                const auto& pdx,
                                const auto& pdv,
                                const auto& cx,
                                const auto& cdx,
                                const auto& cdv) {
        if (!result) {
            return;
        }
        for (Size i = 0; i < px.size(); ++i) {
            if (!almostEqual(px[i], cx[i], eps)) {
                result =
                    makeFailed("Difference in {}\n{} == {}\n\n", getMetadata(id).quantityName, px[i], cx[i]);
                return;
            }
            if (!almostEqual(pdx[i], cdx[i], eps)) {
                result = makeFailed(
                    "Difference in {}\n{} == {}\n\n", getMetadata(id).derivativeName, pdx[i], cdx[i]);
                return;
            }
            if (!almostEqual(pdv[i], cdv[i], eps)) {
                result = makeFailed(
                    "Difference in {}\n{} == {}\n\n", getMetadata(id).secondDerivativeName, pdv[i], cdv[i]);
                return;
            }
        }
    };
    iteratePair<VisitorEnum::SECOND_ORDER>(test, ref, checkSecondOrder);

    return result;
}

Outcome Post::compareLargeSpheres(const Storage& test,
    const Storage& ref,
    const Float fraction,
    const Float maxDeviation,
    const Float eps) {
    ArrayView<const Float> m1 = test.getValue<Float>(QuantityId::MASS);
    ArrayView<const Vector> r1 = test.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Vector> v1 = test.getDt<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m2 = ref.getValue<Float>(QuantityId::MASS);
    ArrayView<const Vector> r2 = ref.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Vector> v2 = ref.getDt<Vector>(QuantityId::POSITION);

    const Size count = Size(max(r1.size(), r2.size()) * fraction);
    if (count >= r1.size() || count >= r2.size()) {
        return makeFailed("Number of particles differs significantly.\nTest has {}\nReference has {}.",
            r1.size(),
            r2.size());
    }

    Order order = getOrder(m2, std::greater<Float>{});
    KdTree<KdNode> tree;
    tree.build(SEQUENTIAL, r1, FinderFlag::SKIP_RANK);

    Array<NeighborRecord> neighs;
    for (Size i = 0; i < count; ++i) {
        // reference index
        const Size p2 = order[i];

        // find test particle
        tree.findAll(r2[p2], maxDeviation, neighs);
        bool matchFound = false;
        for (const NeighborRecord& n : neighs) {
            const Size p1 = n.index;
            if (!almostEqual(m1[p1], m2[p2], eps)) {
                // mass does not match
                continue;
            }
            if (!almostEqual(r1[p1][H], r2[p2][H], eps)) {
                // radius does not match
                continue;
            }
            if (!almostEqual(v1[p1], v2[p2], eps)) {
                // velocities do not match
                continue;
            }
            matchFound = true;
            break;
        }

        if (!matchFound) {
            return makeFailed(
                "No matching test particle found for the {}-th largest particle in the reference state.",
                i + 1);
        }
    }
    return SUCCESS;
}

NAMESPACE_SPH_END
