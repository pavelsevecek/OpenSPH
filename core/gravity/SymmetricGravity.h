#pragma once

/// \file SymmetricGravity.h
/// \brief Wrapper of gravity implementation to be used with symmetric boundary conditions.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "gravity/IGravity.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "system/Settings.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

class SymmetricGravity : public IGravity {
private:
    AutoPtr<IGravity> gravity;
    mutable Storage all;
    Array<Size> idxs;

public:
    explicit SymmetricGravity(AutoPtr<IGravity>&& gravity)
        : gravity(std::move(gravity)) {
        SPH_ASSERT(this->gravity);
    }

    virtual void build(IScheduler& scheduler, const Storage& storage) override {
        if (all.empty()) {
            // lazy init
            Array<Vector> r(2 * storage.getParticleCnt());
            all.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(r));
            all.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 0._f);
        }

        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        Array<Vector>& r_a = all.getValue<Vector>(QuantityId::POSITION);
        Array<Float>& m_a = all.getValue<Float>(QuantityId::MASS);
        r_a.clear();
        m_a.clear();
        idxs.clear();

        for (Size i = 0; i < r.size(); ++i) {
            if (r[i][Z] <= 0._f) {
                // this is the ghost particle created by the boundary conditions
                continue;
            }
            r_a.push(r[i]);
            r_a.push(r[i] - 2._f * Vector(0, 0, r[i][Z]));
            m_a.push(m[i]);
            m_a.push(m[i]);
            idxs.push(i);
            idxs.push(Size(-1));
        }

        gravity->build(scheduler, all);

        Array<Vector>& dv = all.getD2t<Vector>(QuantityId::POSITION);
        dv.resize(r_a.size());
        dv.fill(Vector(0._f));
    }

    virtual void evalSelfGravity(IScheduler& scheduler,
        ArrayView<Vector> dv,
        Statistics& stats) const override {
        ArrayView<Vector> dv_a = all.getD2t<Vector>(QuantityId::POSITION);
        gravity->evalSelfGravity(scheduler, dv_a, stats);

        for (Size i = 0; i < idxs.size(); ++i) {
            if (idxs[i] == Size(-1)) {
                // duplicate
                continue;
            }
            dv[idxs[i]] += dv_a[i];
        }
    }

    virtual void evalExternal(IScheduler& scheduler,
        ArrayView<Attractor> ps,
        ArrayView<Vector> dv) const override {
        gravity->evalExternal(scheduler, ps, dv);
    }

    virtual Vector evalAcceleration(const Vector& r0) const override {
        return gravity->evalAcceleration(r0);
    }

    virtual Float evalEnergy(IScheduler& scheduler, Statistics& stats) const override {
        return gravity->evalEnergy(scheduler, stats);
    }

    virtual RawPtr<const IBasicFinder> getFinder() const override {
        // the tree is built for a different set of particles
        return nullptr;
    }
};

NAMESPACE_SPH_END
