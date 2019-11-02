#include "DeviceMath.h"
#include <assert.h>
#include <stdio.h>
#include <vector>

using namespace Cuda;

__global__ void evalKernel(float3* r, float* m, float3* dv, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;

    if (i >= n) {
        return;
    }
    for (int j = 0; j < n; ++j) {
        if (i == j) {
            continue;
        }
        const float3 dr = r[i] - r[j];
        const float x = getLength(dr);
        dv[i] = dv[i] - m[j] * dr / (x * x * x);
    }
}

extern cudaError_t cudaEvalAll(float3* r, float* m, float3* dv, int n) {

    // printf("cudaEvalAll called with %#016x\n", m);
    // printf("device m = %d\n", m[0]);

    /*printf("cudaEvalAll called with %#016x\n", m);

    printf("N = %d\n", n);
    printf("m0 = %d\n", m[0]);

    printf("size of pointer = %d\n", sizeof(r));


    printf("r0 = %d\n", r[0]);*/

    /*for (int i = 0; i < 3 * n; ++i) {
        printf("INPUT r = %d\n", r[i]);
    }

    for (int i = 0; i < n; ++i) {
        printf("INPUT m = %d\n", m[i]);
    }*/


    const int blockSize = 256;
    const int numBlocks = (n + blockSize - 1) / blockSize;

    evalKernel<<<numBlocks, blockSize>>>(r, m, dv, n);
    // evalKernel<<<numBlocks, blockSize>>>(r, m, dv, n);

    //    cudaDeviceSynchronize();

    return cudaSuccess;
}
