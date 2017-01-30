#pragma once

/// Base class defining interface for kNN queries.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Vector.h"
#include "objects/containers/ArrayView.h"
#include "objects/finders/Order.h"
#include "objects/wrappers/Flags.h"

NAMESPACE_SPH_BEGIN

struct NeighbourRecord {
    Size index;
    Float distanceSqr;

    bool operator!=(const NeighbourRecord& other) const {
        return index != other.index || distanceSqr != other.distanceSqr;
    }
};

enum class FinderFlags {
    /// Finds only neighbours that have smaller smoothing length h than value given
    FIND_ONLY_SMALLER_H = 1 << 0,

    /// Parallelize the search using all available threads
    PARALLELIZE = 1 << 1,
};

namespace Abstract {
    class Finder : public Polymorphic {
    protected:
        ArrayView<const Vector> values;
        Order rankH;

        /// Build finder from set of vectors. This must be called before findNeighbours, can be called more
        /// than once.
        virtual void buildImpl(ArrayView<const Vector> points) = 0;

        /// Rebuild the finder. Can be called only if buildImpl has been called at least once, it can be
        /// therefore a 'lightweight' implementation of build, without allocations etc.
        virtual void rebuildImpl(ArrayView<const Vector> points) = 0;

    public:
        /// Constructs the struct with an array of vectors
        void build(ArrayView<const Vector> points) {
            values = points;
            makeRankH();
            this->buildImpl(values);
        }

        /// Updates the structure when the position change.
        void rebuild() {
            makeRankH();
            this->rebuildImpl(values);
        }

        /// Finds all points within given radius from a point.
        /// \param point
        /// \param radius
        /// \param neighbours List of neighbours, as indices to the array
        /// \param error Approximate solution
        /// \return The number of neighbours.
        virtual Size findNeighbours(const Size index,
            const Float radius,
            Array<NeighbourRecord>& neighbours,
            Flags<FinderFlags> flags = EMPTY_FLAGS,
            const Float error = 0._f) const = 0;

    private:
        void makeRankH() {
            Order tmp(values.size());
            // sort by smoothing length
            tmp.shuffle([this](const Size i1, const Size i2) { return values[i1][H] < values[i2][H]; });
// invert to get rank in H
/// \todo avoid allocation here
#ifdef DEBUG
            Float lastH = 0._f;
            for (Size i = 0; i < tmp.size(); ++i) {
                ASSERT(values[tmp[i]][H] >= lastH);
                lastH = values[tmp[i]][H];
            }
#endif
            rankH = tmp.getInverted();
        }
    };
}

NAMESPACE_SPH_END
