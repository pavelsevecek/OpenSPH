#pragma once

/// \file ShearingSheetFinder.h
/// \brief Finder wrapper adding shearing-sheet image offsets

#include "objects/finders/NeighborFinder.h"
#include "physics/ShearingSheet.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

class ShearingSheetFinder : public ISymmetricFinder {
private:
    AutoPtr<ISymmetricFinder> actual;
    ShearingSheet::Config cfg;
    bool innerRingOnly;
    Float time = 0._f;

    mutable ThreadLocal<Array<NeighborRecord>> extra;

public:
    ShearingSheetFinder(AutoPtr<ISymmetricFinder>&& actual,
        const ShearingSheet::Config& cfg,
        IScheduler& scheduler,
        const bool innerRingOnly = true)
        : actual(std::move(actual))
        , cfg(cfg)
        , innerRingOnly(innerRingOnly)
        , extra(scheduler) {}

    virtual void setTime(const Float t) override {
        time = t;
        actual->setTime(t);
    }

    virtual Size findAll(const Size index, const Float radius, Array<NeighborRecord>& neighbors) const override {
        neighbors.clear();
        this->findImages(values[index], index, radius, neighbors, true);
        return neighbors.size();
    }

    virtual Size findAll(const Vector& pos,
        const Float radius,
        Array<NeighborRecord>& neighbors) const override {
        neighbors.clear();
        this->findImages(pos, values.size(), radius, neighbors, true);
        return neighbors.size();
    }

    virtual Size findLowerRank(const Size index,
        const Float radius,
        Array<NeighborRecord>& neighbors) const override {
        neighbors.clear();
        this->findImages(values[index], index, radius, neighbors, false);
        return neighbors.size();
    }

protected:
    virtual void buildImpl(IScheduler& scheduler, ArrayView<const Vector> points) override {
        actual->build(scheduler, points, FinderFlag::MAKE_RANK);
    }

private:
    void findImages(const Vector& pos,
        const Size index,
        const Float radius,
        Array<NeighborRecord>& neighbors,
        const bool findAll) const {
        ShearingSheet::iterateGhostBoxes(cfg, time, innerRingOnly, [&](const ShearingSheet::GhostBox& gb) {
            Array<NeighborRecord>& local = extra.local();
            local.clear();

            if (all(gb.index == Indices(0))) {
                if (findAll) {
                    if (index < values.size()) {
                        actual->findAll(index, radius, local);
                    } else {
                        actual->findAll(pos, radius, local);
                    }
                } else {
                    actual->findLowerRank(index, radius, local);
                }
            } else {
                actual->findAll(pos - gb.positionOffset, radius, local);
            }

            for (NeighborRecord n : local) {
                if (!findAll && !all(gb.index == Indices(0)) && rank[n.index] >= rank[index]) {
                    continue;
                }
                n.positionOffset = gb.positionOffset;
                n.velocityOffset = gb.velocityOffset;
                neighbors.push(n);
            }
        });
    }
};

NAMESPACE_SPH_END
