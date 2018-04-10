#pragma once

/// Constructing kernels from settings

#include "sph/kernel/GravityKernel.h"
#include "sph/kernel/Kernel.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

namespace Factory {

template <Size D>
LutKernel<D> getKernel(const RunSettings& settings) {
    const KernelEnum id = settings.get<KernelEnum>(RunSettingsId::SPH_KERNEL);
    switch (id) {
    case KernelEnum::CUBIC_SPLINE:
        return CubicSpline<D>();
    case KernelEnum::FOURTH_ORDER_SPLINE:
        return FourthOrderSpline<D>();
    case KernelEnum::GAUSSIAN:
        return Gaussian<D>();
    case KernelEnum::TRIANGLE:
        return TriangleKernel<D>();
    case KernelEnum::CORE_TRIANGLE:
        ASSERT(D == 3);
        return CoreTriangle();
    case KernelEnum::THOMAS_COUCHMAN:
        return ThomasCouchmanKernel<D>();
    case KernelEnum::WENDLAND_C2:
        ASSERT(D == 3);
        return WendlandC2();
    case KernelEnum::WENDLAND_C4:
        ASSERT(D == 3);
        return WendlandC4();
    case KernelEnum::WENDLAND_C6:
        ASSERT(D == 3);
        return WendlandC6();
    default:
        NOT_IMPLEMENTED;
    }
}

inline GravityLutKernel getGravityKernel(const RunSettings& settings) {
    const KernelEnum id = settings.get<KernelEnum>(RunSettingsId::SPH_KERNEL);
    switch (id) {
    case KernelEnum::CUBIC_SPLINE:
        return GravityKernel<CubicSpline<3>>();
    default:
        NOT_IMPLEMENTED;
    }
}

} // namespace Factory

NAMESPACE_SPH_END
