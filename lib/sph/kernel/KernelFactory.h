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
        case KernelEnum::CORE_TRIANGLE:
            ASSERT(D == 3);
            return CoreTriangle();
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
}

NAMESPACE_SPH_END
