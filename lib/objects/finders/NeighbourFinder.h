#pragma once

/// \file INeighbourFinder.h
/// \brief Base class defining interface for kNN queries.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/containers/ArrayView.h"
#include "objects/finders/Order.h"
#include "objects/geometry/Vector.h"
#include "objects/wrappers/Flags.h"

NAMESPACE_SPH_BEGIN

/// \brief Holds information about a neighbour particles
struct NeighbourRecord {
    /// Index of particle in the storage
    Size index;

    /// Squared distance of the particle from the queried particle / position
    Float distanceSqr;

    bool operator!=(const NeighbourRecord& other) const {
        return index != other.index || distanceSqr != other.distanceSqr;
    }

    /// Sort by the distance
    bool operator<(const NeighbourRecord& other) const {
        return distanceSqr < other.distanceSqr;
    }
};

/// \brief Interface of objects finding neighbouring particles.
///
/// Provides queries for searching particles within given radius from given particle or given point in space.
/// Object has to be built before neighbour queries can be made.
class IBasicFinder : public Polymorphic {
protected:
    /// View of the source datapoints, updated every time (re)build is called
    ArrayView<const Vector> values;

public:
    /// \brief Finds all neighbours within given radius from the point given by index.
    ///
    /// Point view passed in \ref build must not be invalidated, in particular the number of particles must
    /// not change before function \ref findAll is called. Note that the particle itself (index-th particle)
    /// is also included in the list of neighbours.
    /// \param index Index of queried particle in the view given in \ref build function.
    /// \param radius Radius in which the neighbours are searched. Must be a positive value.
    /// \param neighbours Output parameter, containing the list of neighbours as indices to the array. The
    ///                   array is cleared by the function.
    /// \return The number of located neighbours.
    virtual Size findAll(const Size index, const Float radius, Array<NeighbourRecord>& neighbours) const = 0;

    /// \brief Finds all points within given radius from given position.
    ///
    /// The position may not correspond to any point.
    virtual Size findAll(const Vector& pos, const Float radius, Array<NeighbourRecord>& neighbours) const = 0;

    /// \brief Constructs the struct with an array of vectors.
    ///
    /// Must be called before \ref findAll is called and before \ref rebuild is called.
    void build(ArrayView<const Vector> points) {
        values = points;
        this->buildImpl(values);
    }

    /// \brief Updates the structure when the positions change.
    ///
    /// Must be called before \ref findAll is called.
    void rebuild() {
        this->rebuildImpl(values);
    }

protected:
    /// \brief Builds finder from set of vectors.
    ///
    /// This must be called before \ref findAll, can be called more than once.
    /// \param points View of the array of points in space.
    virtual void buildImpl(ArrayView<const Vector> points) = 0;

    /// \brief Rebuilds the finder.
    ///
    /// Can be called only if \ref build has been called at least once, it can be therefore a 'lightweight'
    /// implementation of \ref build, without allocations etc.
    virtual void rebuildImpl(ArrayView<const Vector> points) = 0;
};

enum class FinderFlag {
    /// Creates the ranks of particles. Without this flag, only the IBasicFinder interface can be used.
    MAKE_RANK = 1 << 0,

    /// The rank of particles is not created. 'Dummy' option that can be used to improve readability.
    SKIP_RANK = 0,
};

/// \brief Extension of IBasicFinder, allowing to search only particles with lower rank in smoothing length.
///
/// This is useful to find each pair of neighbouring particles only once; if i-th particle 'sees' j-th
/// particle, j-th particle does not 'see' i-th particle. This can be a significant optimization as only half
/// of the neighbours is evaluated.
class ISymmetricFinder : public IBasicFinder {
    friend class DynamicFinder;

protected:
    /// Ranks of particles according to their smoothing lengths
    Order rankH;

public:
    /// \brief Constructs the struct with an array of vectors.
    void build(ArrayView<const Vector> points, const Flags<FinderFlag> flags = FinderFlag::MAKE_RANK) {
        values = points;
        if (flags.has(FinderFlag::MAKE_RANK)) {
            rankH = makeRankH(values);
        } else {
            rankH = Order();
        }
        this->buildImpl(values);
    }

    /// \brief Updates the structure when the position change.
    void rebuild(const Flags<FinderFlag> flags = FinderFlag::MAKE_RANK) {
        if (flags.has(FinderFlag::MAKE_RANK)) {
            rankH = makeRankH(values);
        } else {
            rankH = Order();
        }
        this->rebuildImpl(values);
    }

    /// \brief Finds all points within radius that have a lower rank in smoothing length.
    ///
    /// The sorting of particles with equal smoothing length is not specified, but it is ensured that all
    /// found neighbours will not find the queries particle if \ref findAsymmetric is called with that
    /// particle. Note that this does NOT find the queried particle itself.
    /// \param index Index of queried particle in the view given in \ref build function.
    /// \param radius Radius in which the neighbours are searched. Must be a positive value.
    /// \param neighbours Output parameter, containing the list of neighbours as indices to the array. The
    ///                   array is cleared by the function.
    /// \return The number of located neighbours. Can be zero.
    virtual Size findLowerRank(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const = 0;

private:
    /// \brief Generates the rank of particles, according to their smoothing lengths.
    static Order makeRankH(ArrayView<const Vector> values) {
        Order tmp(values.size());
        // sort by smoothing length
        tmp.shuffle([values](const Size i1, const Size i2) { return values[i1][H] < values[i2][H]; });
// invert to get rank in H
/// \todo avoid allocation here
#ifdef SPH_DEBUG
        Float lastH = EPS;
        for (Size i = 0; i < tmp.size(); ++i) {
            ASSERT(values[tmp[i]][H] >= lastH, values[tmp[i]][H], lastH);
            lastH = values[tmp[i]][H];
        }
#endif
        return tmp.getInverted();
    }
};

/// \brief Helper template, allowing to define all three functions with a single function.
template <typename TDerived>
class FinderTemplate : public ISymmetricFinder {
public:
    virtual Size findAll(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override {
        neighbours.clear();
        return static_cast<const TDerived*>(this)->template find<true>(
            values[index], index, radius, neighbours);
    }

    virtual Size findAll(const Vector& pos,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override {
        neighbours.clear();
        if (SPH_UNLIKELY(values.empty())) {
            return 0._f;
        }
        // the index here is irrelevant, so let's use something that would cause assert in case we messed
        // something up
        const Size index = values.size();
        return static_cast<const TDerived*>(this)->template find<true>(pos, index, radius, neighbours);
    }

    virtual Size findLowerRank(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override {
        neighbours.clear();
        return static_cast<const TDerived*>(this)->template find<false>(
            values[index], index, radius, neighbours);
    }
};

NAMESPACE_SPH_END
