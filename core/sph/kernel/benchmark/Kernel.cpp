#include "sph/kernel/Kernel.h"
#include "bench/Session.h"
#include "math/rng/Rng.h"

using namespace Sph;

BENCHMARK("LutKernel sequential", "[kernel]", Benchmark::Context& context) {
    LutKernel<3> kernel(CubicSpline<3>{});
    while (context.running()) {
        for (Float r = 0.f; r < kernel.radius(); r += 1.e-4_f) {
            Benchmark::doNotOptimize(kernel.value(Vector(r, 0.f, 0.f), 1.f));
            Benchmark::doNotOptimize(kernel.grad(Vector(0.f, r, 0.f), 1.f));
            Benchmark::clobberMemory();
        }
    }
}

BENCHMARK("LutKernel random", "[kernel]", Benchmark::Context& context) {
    LutKernel<3> kernel(CubicSpline<3>{});
    UniformRng rng;
    while (context.running()) {
        for (Size i = 0; i < 1e4; ++i) {
            Benchmark::doNotOptimize(kernel.value(Vector(3._f * rng(), 0.f, 0.f), 1.f));
            Benchmark::doNotOptimize(kernel.grad(Vector(0.f, 3._f * rng(), 0.f), 1.f));
            Benchmark::clobberMemory();
        }
    }
}
