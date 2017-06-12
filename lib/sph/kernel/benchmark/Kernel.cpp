#include "sph/kernel/Kernel.h"
#include "bench/Session.h"

using namespace Sph;

BENCHMARK("LutKernel value", "[kernel]", Benchmark::Context& context) {
    LutKernel<3> kernel(CubicSpline<3>{});
    while (context.running()) {
        for (Float r = 0.f; r < kernel.radius(); r += 0.01_f) {
            kernel.value(Vector(r, 0.f, 0.f), 1.f);
        }
    }
}

BENCHMARK("LutKernel grad", "[kernel]", Benchmark::Context& context) {
    LutKernel<3> kernel(CubicSpline<3>{});
    while (context.running()) {
        for (Float r = 0.f; r < kernel.radius(); r += 0.01_f) {
            kernel.grad(Vector(r, 0.f, 0.f), 1.f);
        }
    }
}
