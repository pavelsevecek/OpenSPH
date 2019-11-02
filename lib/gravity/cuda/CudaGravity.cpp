#include "gravity/cuda/CudaGravity.h"
#include "system/Timer.h"
#include <cuda_runtime.h>
#include <iostream>
#include <vector>

extern cudaError_t cudaEvalAll(float3* r, float* m, float3* dv, int n);

NAMESPACE_SPH_BEGIN

#define SPH_CHECK_CUDA(func)                                                                                 \
    {                                                                                                        \
        cudaError_t result = func;                                                                           \
        ASSERT(result == cudaSuccess, cudaGetErrorName(result));                                             \
    }

void CudaGravity::evalAll(IScheduler& UNUSED(scheduler),
    ArrayView<Vector> dv,
    Statistics& UNUSED(stats)) const {

    std::vector<float> rf(3 * r.size());
    std::vector<float> mf(r.size());
    std::vector<float> dvf(3 * r.size());
    for (Size i = 0; i < m.size(); ++i) {
        mf[i] = Constants::gravity * m[i];
        for (Size j = 0; j < 3; ++j) {
            rf[3 * i + j] = r[i][j];
            dvf[3 * i + j] = 0.f;
        }
    }

    const int n = r.size();

    float3* r_d;
    float3* dv_d;
    float* m_d;

    SPH_CHECK_CUDA(cudaMalloc((void**)&r_d, n * sizeof(float3)));
    SPH_CHECK_CUDA(cudaMalloc((void**)&m_d, n * sizeof(float)));
    SPH_CHECK_CUDA(cudaMalloc((void**)&dv_d, n * sizeof(float3)));

    SPH_CHECK_CUDA(cudaMemcpy(r_d, rf.data(), 3 * n * sizeof(float), cudaMemcpyHostToDevice));
    SPH_CHECK_CUDA(cudaMemcpy(m_d, mf.data(), n * sizeof(float), cudaMemcpyHostToDevice));
    SPH_CHECK_CUDA(cudaMemcpy(dv_d, dvf.data(), 3 * n * sizeof(float), cudaMemcpyHostToDevice));


    std::cout << "Calling cudaEvalAll with " << m_d << std::endl;

    // std::cout << "m[0] = " << mf[0] << std:://endl;
    // std::cout << "m[0] = " << mf[0] << std::endl;
    {
        Timer timer;
        SPH_CHECK_CUDA(cudaEvalAll(r_d, m_d, dv_d, mf.size()));
        SPH_CHECK_CUDA(cudaDeviceSynchronize());
        std::cout << "Kernel took " << timer.elapsed(TimerUnit::MILLISECOND) << "ms" << std::endl;
    }

    SPH_CHECK_CUDA(cudaMemcpy(dvf.data(), dv_d, 3 * n * sizeof(float), cudaMemcpyDeviceToHost));


    SPH_CHECK_CUDA(cudaFree(r_d));
    SPH_CHECK_CUDA(cudaFree(m_d));
    SPH_CHECK_CUDA(cudaFree(dv_d));

    for (Size i = 0; i < m.size(); ++i) {
        dv[i] = Vector(dvf[3 * i], dvf[3 * i + 1], dvf[3 * i + 2]);
    }

    std::cout << "ALL DONE" << std::endl;
}

NAMESPACE_SPH_END
