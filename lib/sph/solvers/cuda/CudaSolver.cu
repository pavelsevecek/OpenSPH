#include <cuda_runtime.h>

__global__ void test_kernel(void) {
}

extern "C" void runCuda(float3* v) {
    test_kernel <<<1, 1 >>> ();
}
