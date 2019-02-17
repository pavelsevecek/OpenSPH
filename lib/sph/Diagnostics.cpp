#include "sph/Diagnostics.h"
#include "objects/finders/NeighbourFinder.h"
#include "quantities/IMaterial.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

Array<ParticlePairingDiagnostic::Pair> ParticlePairingDiagnostic::getPairs(const Storage& storage) const {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    AutoPtr<ISymmetricFinder> finder = Factory::getFinder(RunSettings::getDefaults());
    finder->build(SEQUENTIAL, r);

    Array<ParticlePairingDiagnostic::Pair> pairs;
    Array<NeighbourRecord> neighs;

    /// \todo symmetrized h
    for (Size i = 0; i < r.size(); ++i) {
        // only smaller h to go through each pair only once
        finder->findLowerRank(i, r[i][H] * radius, neighs);
        for (auto& n : neighs) {
            if (getSqrLength(r[i] - r[n.index]) < sqr(limit * (r[i][H] + r[n.index][H]))) {
                pairs.push(ParticlePairingDiagnostic::Pair{ i, n.index });
            }
        }
    }
    return pairs;
}

DiagnosticsReport ParticlePairingDiagnostic::check(const Storage& storage,
    const Statistics& UNUSED(stats)) const {
    Array<Pair> pairs = getPairs(storage);
    if (pairs.empty()) {
        return SUCCESS;
    } else {
        DiagnosticsError error;
        error.description = "Particle pairs found";
        for (Pair& p : pairs) {
            error.offendingParticles[p.i1]; /// \todo some message? compare it to the limit?
            error.offendingParticles[p.i2];
        }
        return error;
    }
}

DiagnosticsReport SmoothingDiscontinuityDiagnostic::check(const Storage& storage,
    const Statistics& UNUSED(stats)) const {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    AutoPtr<ISymmetricFinder> finder = Factory::getFinder(RunSettings::getDefaults());
    finder->build(SEQUENTIAL, r);
    Array<NeighbourRecord> neighs;

    struct Pair {
        Size i1, i2;
    };
    Array<Pair> pairs;
    for (Size i = 0; i < r.size(); ++i) {
        finder->findLowerRank(i, r[i][H] * radius, neighs);
        for (auto& n : neighs) {
            const Size j = n.index;
            if (abs(r[i][H] - r[j][H]) > limit * (r[i][H] + r[j][H])) {
                pairs.push(Pair{ i, j });
            }
        }
    }
    if (pairs.empty()) {
        return SUCCESS;
    } else {
        DiagnosticsError error;
        error.description = "Discontinuity in smoothing lengths found";
        for (Pair& p : pairs) {
            const Float actual = abs(r[p.i1][H] - r[p.i2][H]);
            const Float expected = limit * (r[p.i1][H] + r[p.i2][H]);
            const std::string message =
                "dH = " + std::to_string(actual) + " (limit = " + std::to_string(expected) + ")";
            error.offendingParticles[p.i1] = message;
            error.offendingParticles[p.i2] = message;
        }
        return error;
    }
}

CourantInstabilityDiagnostic::CourantInstabilityDiagnostic(const Float timescaleFactor)
    : factor(timescaleFactor) {}

DiagnosticsReport CourantInstabilityDiagnostic::check(const Storage& storage,
    const Statistics& UNUSED(stats)) const {
    const Float dt2 = sqr(factor);
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    DiagnosticsError error;
    error.description = "Probable CFL instability detected";
    for (Size i = 0; i < r.size(); ++i) {
        const Float limit = r[i][H] / dt2;
        if (getLength(dv[i]) > limit) {
            error.offendingParticles[i] =
                "dv = " + std::to_string(getLength(dv[i])) + " (limit = " + std::to_string(limit) + ")";
        }
    }
    if (error.offendingParticles.empty()) {
        return SUCCESS;
    } else {
        return error;
    }
}

DiagnosticsReport OvercoolingDiagnostic::check(const Storage& storage, const Statistics& stats) const {
    const Float dt = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
    ArrayView<const Float> u, du;
    tie(u, du) = storage.getAll<Float>(QuantityId::ENERGY);
    DiagnosticsError error;
    error.description = "Particle overcooling detected";
    for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
        MaterialView material = storage.getMaterial(matId);
        const Interval range = material->range(QuantityId::ENERGY);
        for (Size i : material.sequence()) {
            const Float u1 = u[i] + du[i] * dt;
            if (u1 < range.lower()) {
                error.offendingParticles[i] =
                    "u = " + std::to_string(u1) + " (limit = " + std::to_string(range.lower()) + ")";
            }
        }
    }
    if (error.offendingParticles.empty()) {
        return SUCCESS;
    } else {
        return error;
    }
}

NAMESPACE_SPH_END
