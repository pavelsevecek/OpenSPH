#include "sph/Diagnostics.h"
#include "objects/finders/KdTree.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

Array<ParticlePairing::Pair> ParticlePairing::getPairs(const Storage& storage) const {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    KdTree finder;
    finder.build(SEQUENTIAL, r);

    Array<ParticlePairing::Pair> pairs;
    Array<NeighbourRecord> neighs;

    /// \todo symmetrized h
    for (Size i = 0; i < r.size(); ++i) {
        // only smaller h to go through each pair only once
        finder.findLowerRank(i, r[i][H] * radius, neighs);
        for (auto& n : neighs) {
            if (getSqrLength(r[i] - r[n.index]) < sqr(limit * (r[i][H] + r[n.index][H]))) {
                pairs.push(ParticlePairing::Pair{ i, n.index });
            }
        }
    }
    return pairs;
}

Outcome ParticlePairing::check(const Storage& storage) {
    Array<Pair> pairs = getPairs(storage);
    if (!pairs.empty()) {
        std::string message = "Particle pairs found: \n";
        for (Pair& p : pairs) {
            message += " - particles " + std::to_string(p.i1) + " and " + std::to_string(p.i2) + "\n";
        }
        return message;
    }
    return SUCCESS;
}

Outcome SmoothingDiscontinuity::check(const Storage& storage) {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    KdTree finder;
    finder.build(SEQUENTIAL, r);
    Array<NeighbourRecord> neighs;

    struct Pair {
        Size i1, i2;
    };
    Array<Pair> pairs;
    for (Size i = 0; i < r.size(); ++i) {
        finder.findLowerRank(i, r[i][H] * radius, neighs);
        for (auto& n : neighs) {
            const Size j = n.index;
            if (abs(r[i][H] - r[j][H]) > limit * (r[i][H] + r[j][H])) {
                pairs.push(Pair{ i, j });
            }
        }
    }
    if (!pairs.empty()) {
        std::string message = "Discontinuity in smoothing lengths found: \n";
        for (Pair& p : pairs) {
            message += " - particles " + std::to_string(p.i1) + " and " + std::to_string(p.i2) + "\n";
        }
        return message;
    }
    return SUCCESS;
}

NAMESPACE_SPH_END
