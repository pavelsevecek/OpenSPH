#pragma once

/// \file ShearingSheetGravity.h
/// \brief Gravity wrapper adding shearing-sheet ghost boxes

#include "gravity/IGravity.h"
#include "physics/ShearingSheet.h"

NAMESPACE_SPH_BEGIN

class ShearingSheetGravity : public IGravity {
private:
    ShearingSheet::Config cfg;
    AutoPtr<IGravity> actual;
    ArrayView<const Vector> positions;
    Float time = 0._f;

public:
    ShearingSheetGravity(const ShearingSheet::Config& cfg, AutoPtr<IGravity>&& actual)
        : cfg(cfg)
        , actual(std::move(actual)) {}

    virtual void setTime(const Float t) override {
        time = t;
        actual->setTime(t);
    }

    virtual void build(IScheduler& scheduler, const Storage& storage) override {
        positions = storage.getValue<Vector>(QuantityId::POSITION);
        actual->build(scheduler, storage);
    }

    virtual void evalSelfGravity(IScheduler& scheduler, ArrayView<Vector> dv, Statistics& stats) const override {
        actual->evalSelfGravity(scheduler, dv, stats);
        parallelFor(scheduler, 0, dv.size(), [this, &dv](const Size i) {
            ShearingSheet::iterateGhostBoxes(cfg, time, false, [&](const ShearingSheet::GhostBox& gb) {
                if (all(gb.index == Indices(0))) {
                    return;
                }
                dv[i] += actual->evalAcceleration(clearH(positions[i]) - gb.positionOffset);
            });
        });
    }

    virtual void evalAttractors(IScheduler& scheduler,
        ArrayView<Attractor> attractors,
        ArrayView<Vector> dv) const override {
        actual->evalAttractors(scheduler, attractors, dv);
    }

    virtual Vector evalAcceleration(const Vector& r0) const override {
        Vector a = actual->evalAcceleration(r0);
        ShearingSheet::iterateGhostBoxes(cfg, time, false, [&](const ShearingSheet::GhostBox& gb) {
            if (all(gb.index == Indices(0))) {
                return;
            }
            a += actual->evalAcceleration(r0 - gb.positionOffset);
        });
        return a;
    }

    virtual Float evalEnergy(IScheduler& scheduler, Statistics& stats) const override {
        return actual->evalEnergy(scheduler, stats);
    }

    virtual RawPtr<const IBasicFinder> getFinder() const override {
        return actual->getFinder();
    }
};

NAMESPACE_SPH_END
