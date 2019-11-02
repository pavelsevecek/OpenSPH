#include "sph/solvers/cuda/CudaSolver.h"
#include <cuda_runtime.h>
#include <vector>

extern "C" void runCuda(float3* v);

NAMESPACE_SPH_BEGIN

void CudaSolver::integrate(Storage& storage, Statistics& UNUSED(stats)) {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    std::vector<float3> v(r.size());
    for (Size i = 0; i < r.size(); ++i) {
        v[i] = { float(r[i][X]), float(r[i][Y]), float(r[i][Z]) };
    }
    runCuda(v.data());
}

NAMESPACE_SPH_END
