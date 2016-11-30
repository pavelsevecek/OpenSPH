#include "solvers/ContinuitySolver.h"
#include "objects/containers/ArrayUtils.h"
#include "objects/finders/Finder.h"
#include "physics/Eos.h"
#include "sph/distributions/Distribution.h"
#include "storage/Iterate.h"
#include "system/Factory.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN



template <int d>
void ContinuitySolver<d>::compute(Storage& storage) {
    const int size = storage.getParticleCnt();

    // Check that quantities are valid
    ASSERT(areAllMatching(dv, [](const Vector v) { return v == Vector(0._f); }));
    ASSERT(areAllMatching(rho, [](const Float v) { return v > 0._f; }));

    // build neighbour finding object
    /// \todo only rebuild(), try to limit allocations
    PROFILE_NEXT("ContinuitySolver::compute (init)")
    finder->build(r);

    // we symmetrize kernel by averaging smoothing lenghts
    SymH<d> w(kernel);

    PROFILE_NEXT("ContinuitySolver::compute (main cycle)")
    for (int i = 0; i < size; ++i) {
        // Find all neighbours within kernel support. Since we are only searching for particles with
        // smaller h, we know that symmetrized lengths (h_i + h_j)/2 will be ALWAYS smaller or equal to
        // h_i, and we thus never "miss" a particle.
        SCOPE_STOP;
        finder->findNeighbours(i, r[i][H] * kernel.radius(), neighs, FinderFlags::FIND_ONLY_SMALLER_H);
        SCOPE_RESUME;
        // iterate over neighbours
        for (const auto& neigh : neighs) {
            const int j = neigh.index;
            // actual smoothing length
            const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
            ASSERT(hbar > EPS && hbar <= r[i][H]);
            if (getSqrLength(r[i] - r[j]) > Math::sqr(kernel.radius() * hbar)) {
                // aren't actual neighbours
                continue;
            }
            // compute gradient of kernel W_ij
            const Vector grad = w.grad(r[i], r[j]);
            ASSERT(dot(grad, r[i] - r[j]) <= 0._f);

            const Vector f = force(i, j);

            ASSERT(Math::isReal(f));
            dv[i] -= m[j] * f; // opposite sign due to antisymmetry of gradient
            dv[j] += m[i] * f;

            process(grad); // nothing should depend on Wij
        }
    }
}


/// instantiate classes
template class ContinuitySolver<1>;
template class ContinuitySolver<2>;
template class ContinuitySolver<3>;

NAMESPACE_SPH_END
