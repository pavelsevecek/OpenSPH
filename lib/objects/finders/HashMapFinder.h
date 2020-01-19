#pragma once

/// \file HashMapFinder.h
/// \brief Finding neighbors using hash map
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz))
/// \date 2016-2019

#include "objects/finders/NeighbourFinder.h"
#include "objects/geometry/Box.h"
#include "system/Settings.h"
#include <unordered_map>

NAMESPACE_SPH_BEGIN

class HashMapFinder : public FinderTemplate<HashMapFinder> {
private:
    /// \todo deduplicate
    class IndicesHash {
    public:
        INLINE std::size_t operator()(const Indices& idxs) const {
            std::hash<int> hash;
            return combine(combine(hash(idxs[0]), hash(idxs[1])), hash(idxs[2]));
        }

    private:
        INLINE size_t combine(const size_t lhs, const size_t rhs) const {
            return lhs ^ (rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2));
        }
    };
    class IndicesEqual {
    public:
        INLINE bool operator()(const Indices& i1, const Indices& i2) const {
            return all(i1 == i2);
        }
    };

    struct Cell {
        Array<Size> points;
        Box box;

        Cell();
        ~Cell();
    };

    std::unordered_map<Indices, Cell, IndicesHash, IndicesEqual> map;
    Float cellSize;
    Float kernelRadius;


public:
    HashMapFinder(const RunSettings& settings);

    ~HashMapFinder();

    template <bool FindAll>
    Size find(const Vector& pos, const Size index, const Float radius, Array<NeighbourRecord>& neighs) const;

protected:
    virtual void buildImpl(IScheduler& scheduler, ArrayView<const Vector> points) override;
};

NAMESPACE_SPH_END
