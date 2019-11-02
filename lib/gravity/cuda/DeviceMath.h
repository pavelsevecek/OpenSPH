#pragma once

#include <cuda_runtime.h>

namespace Cuda {

__host__ __device__ float3 operator+(const float3 v1, const float3 v2) {
    return make_float3(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z);
}

__host__ __device__ float3 operator-(const float3 v1, const float3 v2) {
    return make_float3(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
}

__host__ __device__ float3 operator*(const float3 v, const float m) {
    return make_float3(v.x * m, v.y * m, v.z * m);
}

__host__ __device__ float3 operator*(const float m, const float3 v) {
    return make_float3(v.x * m, v.y * m, v.z * m);
}

__host__ __device__ float3 operator/(const float3 v, const float m) {
    return make_float3(v.x / m, v.y / m, v.z / m);
}

__host__ __device__ float dot(const float3 v1, const float3 v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

__host__ __device__ float getSqrLength(const float3 v) {
    return dot(v, v);
}

__host__ __device__ float getLength(const float3 v) {
    return sqrt(getSqrLength(v));
}

} // namespace Cuda
