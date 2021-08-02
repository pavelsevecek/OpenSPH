#pragma once

/// \file HashMapFinder.h
/// \brief Finding neighbors using hash map
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz))
/// \date 2016-2021

#include "math/Means.h"
#include "objects/finders/NeighborFinder.h"
#include "objects/geometry/Box.h"
#include "system/Settings.h"
#include <unordered_map>

NAMESPACE_SPH_BEGIN

class HashMapFinder : public FinderTemplate<HashMapFinder> {
public:
    struct Cell {
        Array<Size> points;
        Box box;

        Cell();
        ~Cell();
    };

private:
    std::unordered_map<Indices, Cell, std::hash<Indices>, IndicesEqual> map;
    Float cellSize;
    Float kernelRadius;
    Float cellMult;

public:
    HashMapFinder(const RunSettings& settings, const Float cellMult = 1._f);

    ~HashMapFinder();

    template <bool FindAll>
    Size find(const Vector& pos, const Size index, const Float radius, Array<NeighborRecord>& neighs) const;

    template <typename TFunctor>
    void iterate(const TFunctor& func) const {
        for (auto& p : map) {
            Vector lower = Vector(p.first) * cellSize;
            // lower += (Vector(-1._f) - Vector(p.first < Indices(0))) * cellSize;
            const Box box(lower, lower + Vector(Indices(1, 1, 1)) * cellSize);
            func(p.second, box);
        }
    }

    /// \brief Checks if the particles are equally distributed among buckets.
    Outcome good(const Size maxBucketSize) const;

    MinMaxMean getBucketStats() const;

protected:
    virtual void buildImpl(IScheduler& scheduler, ArrayView<const Vector> points) override;
};

NAMESPACE_SPH_END
