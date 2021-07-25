#pragma once

/// \file NeighborFinder.h
/// \brief Base class defining interface for kNN queries.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/containers/ArrayView.h"
#include "objects/finders/Order.h"
#include "objects/geometry/Vector.h"
#include "objects/wrappers/Flags.h"

NAMESPACE_SPH_BEGIN

class IScheduler;

/// \brief Holds information about a neighbor particles
struct NeighborRecord {
    /// Index of particle in the storage
    Size index;

    /// Squared distance of the particle from the queried particle / position
    Float distanceSqr;

    bool operator!=(const NeighborRecord& other) const {
        return index != other.index || distanceSqr != other.distanceSqr;
    }

    /// Sort by the distance
    bool operator<(const NeighborRecord& other) const {
        return distanceSqr < other.distanceSqr;
    }
};

/// \brief Interface of objects finding neighboring particles.
///
/// Provides queries for searching particles within given radius from given particle or given point in space.
/// Object has to be built before neighbor queries can be made.
class IBasicFinder : public Polymorphic {
protected:
    /// View of the source datapoints, updated every time (re)build is called
    ArrayView<const Vector> values;

public:
    /// \brief Constructs the struct with an array of vectors.
    ///
    /// Must be called before \ref findAll is called or if the referenced array is invalidated.
    /// \param scheduler Scheduler that can be used for parallelization.
    /// \param points View of the array of points in space.
    void build(IScheduler& scheduler, ArrayView<const Vector> points);

    /// \brief Finds all neighbors within given radius from the point given by index.
    ///
    /// Point view passed in \ref build must not be invalidated, in particular the number of particles must
    /// not change before function \ref findAll is called. Note that the particle itself (index-th particle)
    /// is also included in the list of neighbors.
    /// \param index Index of queried particle in the view given in \ref build function.
    /// \param radius Radius in which the neighbors are searched. Must be a positive value.
    /// \param neighbors Output parameter, containing the list of neighbors as indices to the array. The
    ///                  array is cleared by the function.
    /// \return The number of located neighbors.
    virtual Size findAll(const Size index, const Float radius, Array<NeighborRecord>& neighbors) const = 0;

    /// \brief Finds all points within given radius from given position.
    ///
    /// The position may not correspond to any point.
    virtual Size findAll(const Vector& pos, const Float radius, Array<NeighborRecord>& neighbors) const = 0;

protected:
    /// \brief Builds finder from set of vectors.
    ///
    /// This must be called before \ref findAll, can be called more than once.
    /// \param scheduler Scheduler that can be used for parallelization.
    /// \param points View of the array of points in space.
    virtual void buildImpl(IScheduler& scheduler, ArrayView<const Vector> points) = 0;
};

enum class FinderFlag {
    /// Creates the ranks of particles. Without this flag, only the IBasicFinder interface can be used.
    MAKE_RANK = 1 << 0,

    /// The rank of particles is not created. 'Dummy' option that can be used to improve readability.
    SKIP_RANK = 0,
};

/// \brief Generates the ranks of particles, according to generic predicate.
template <typename TCompare>
static Order makeRank(const Size size, TCompare&& comp) {
    Order tmp(size);
    // sort using the given comparator
    tmp.shuffle(comp);
    // invert to get the rank
    return tmp.getInverted();
}

/// \brief Extension of IBasicFinder, allowing to search only particles with lower rank in smoothing length.
///
/// This is useful to find each pair of neighboring particles only once; if i-th particle 'sees' j-th
/// particle, j-th particle does not 'see' i-th particle. This can be a significant optimization as only half
/// of the neighbors is evaluated.
class ISymmetricFinder : public IBasicFinder {
protected:
    /// Ranks of particles according to their smoothing lengths
    Order rank;

public:
    /// \brief Constructs the struct with an array of vectors.
    void build(IScheduler& scheduler,
        ArrayView<const Vector> points,
        Flags<FinderFlag> flags = FinderFlag::MAKE_RANK);

    /// \brief Constructs the struct with custom predicate for ordering particles.
    template <typename TCompare>
    void buildWithRank(IScheduler& scheduler, ArrayView<const Vector> points, TCompare&& comp) {
        values = points;
        rank = makeRank(values.size(), comp);
        this->buildImpl(scheduler, values);
    }

    /// \brief Finds all points within radius that have a lower rank in smoothing length.
    ///
    /// The sorting of particles with equal smoothing length is not specified, but it is ensured that all
    /// found neighbors will not find the queries particle if \ref findAsymmetric is called with that
    /// particle. Note that this does NOT find the queried particle itself.
    /// \param index Index of queried particle in the view given in \ref build function.
    /// \param radius Radius in which the neighbors are searched. Must be a positive value.
    /// \param neighbors Output parameter, containing the list of neighbors as indices to the array. The
    ///                   array is cleared by the function.
    /// \return The number of located neighbors. Can be zero.
    virtual Size findLowerRank(const Size index,
        const Float radius,
        Array<NeighborRecord>& neighbors) const = 0;
};

/// \brief Helper template, allowing to define all three functions with a single function.
template <typename TDerived>
class FinderTemplate : public ISymmetricFinder {
public:
    virtual Size findAll(const Size index,
        const Float radius,
        Array<NeighborRecord>& neighbors) const override {
        neighbors.clear();
        return static_cast<const TDerived*>(this)->template find<true>(
            values[index], index, radius, neighbors);
    }

    virtual Size findAll(const Vector& pos,
        const Float radius,
        Array<NeighborRecord>& neighbors) const override {
        neighbors.clear();
        if (SPH_UNLIKELY(values.empty())) {
            return 0._f;
        }
        // the index here is irrelevant, so let's use something that would cause assert in case we messed
        // something up
        const Size index = values.size();
        return static_cast<const TDerived*>(this)->template find<true>(pos, index, radius, neighbors);
    }

    virtual Size findLowerRank(const Size index,
        const Float radius,
        Array<NeighborRecord>& neighbors) const override {
        neighbors.clear();
        return static_cast<const TDerived*>(this)->template find<false>(
            values[index], index, radius, neighbors);
    }
};

NAMESPACE_SPH_END
