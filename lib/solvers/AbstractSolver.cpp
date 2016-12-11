#include "solvers/AbstractSolver.h"
#include "objects/containers/ArrayUtils.h"
#include "physics/Eos.h"

NAMESPACE_SPH_BEGIN

void Abstract::Solver::computeMaterial(Storage& storage) {
    ArrayView<Float> p, rho, u, cs;
    tieToArray(p, cs, rho, u) =
        storage.getValues<Float>(QuantityKey::P, QuantityKey::CS, QuantityKey::RHO, QuantityKey::U);

    for (int i = 0; i < storage.getParticleCnt(); ++i) {
        Material& mat = storage.getMaterial(i);
        tie(p[i], cs[i]) = mat.eos->getPressure(rho[i], u[i]);
    }
    ASSERT(areAllMatching(rho, [](const Float v) { return v > 0._f; }));
}

NAMESPACE_SPH_END
