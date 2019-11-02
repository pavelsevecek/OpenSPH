#pragma once

#include "gravity/IGravity.h"
#include "physics/Constants.h"
#include "quantities/Storage.h"
#include "sph/kernel/GravityKernel.h"

NAMESPACE_SPH_BEGIN

class CudaGravity : public IGravity {
private:
    ArrayView<const Vector> r;
    ArrayView<const Float> m;

    GravityLutKernel kernel;
    Float gravityConstant = Constants::gravity;

public:
    CudaGravity(const Float gravityContant = Constants::gravity)
        : gravityConstant(gravityContant) {
        ASSERT(kernel.radius() == 0._f);
    }

    CudaGravity(GravityLutKernel&& kernel, const Float gravityContant = Constants::gravity)
        : kernel(std::move(kernel))
        , gravityConstant(gravityContant) {}

    virtual void build(IScheduler& UNUSED(scheduler), const Storage& storage) override {
        r = storage.getValue<Vector>(QuantityId::POSITION);
        m = storage.getValue<Float>(QuantityId::MASS);
    }

    virtual void evalAll(IScheduler& scheduler, ArrayView<Vector> dv, Statistics& stats) const override;

    virtual Vector eval(const Vector& UNUSED(r0)) const override {
        return Vector(0._f);
    }

    virtual Float evalEnergy(IScheduler& UNUSED(scheduler), Statistics& UNUSED(stats)) const override {
        return 0._f;
    }

    virtual RawPtr<const IBasicFinder> getFinder() const override {
        return nullptr;
    }
};

NAMESPACE_SPH_END
