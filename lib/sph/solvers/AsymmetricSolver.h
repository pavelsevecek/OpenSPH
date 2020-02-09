#pragma once

/// \file AsymmetricSolver.h
/// \brief SPH solver with asymmetric particle evaluation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/geometry/Indices.h"
#include "sph/equations/Derivative.h"
#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"
#include "thread/ThreadLocal.h"
#include "timestepping/ISolver.h"
#include <unordered_map>

NAMESPACE_SPH_BEGIN

class ISymmetricFinder;
class IBoundaryCondition;

/// \brief Helper structure storing search radii for particles as hash map.
class RadiiHashMap {
private:
    class IndicesEqual {
    public:
        INLINE bool operator()(const Indices& i1, const Indices& i2) const {
            return all(i1 == i2);
        }
    };

    std::unordered_map<Indices, Float, std::hash<Indices>, IndicesEqual> map;
    Float cellSize;

public:
    /// \brief Computes the search radii at each cell in space.
    /// \param r Positions and smoothing lenghts of particles.
    /// \param kernelRadius Dimensionless support radius of the kernel.
    void build(ArrayView<const Vector> r, const Float kernelRadius);

    /// \brief Returns the required search radius for particle at given position.
    Float getRadius(const Vector& r) const;
};

/// \brief Base class for asymmetric SPH solvers.
class IAsymmetricSolver : public ISolver {
protected:
    /// \brief Structure used to search for neighbouring particles.
    ///
    /// Should not be accessed by derived classes directly, use \ref getFinder instead.
    AutoPtr<ISymmetricFinder> finder;

    /// Scheduler used to parallelize the solver.
    IScheduler& scheduler;

    /// Holds all equation terms evaluated by the solver.
    EquationHolder equations;

    /// Selected SPH kernel
    LutKernel<DIMENSIONS> kernel;

    /// Hash map used to determine search radii of particles.
    Optional<RadiiHashMap> radiiMap;

public:
    IAsymmetricSolver(IScheduler& scheduler, const RunSettings& settings, const EquationHolder& eqs);

    virtual void integrate(Storage& storage, Statistics& stats) override;

    virtual void create(Storage& storage, IMaterial& material) const override;

protected:
    Float getMaxSearchRadius(const Storage& storage) const;

    /// \brief Returns a finder, already build using the provided positions.
    ///
    /// Derived classes might override this function to return generally different finder that the member \ref
    /// finder, implementations should therefore use this function instead of accessing the finder directly.
    virtual RawPtr<const IBasicFinder> getFinder(ArrayView<const Vector> r);

    /// \todo this structure is probably not needed, we only use it by EnergyConservingSolver anyway
    virtual void beforeLoop(Storage& storage, Statistics& stats) = 0;

    virtual void loop(Storage& storage, Statistics& stats) = 0;

    virtual void afterLoop(Storage& storage, Statistics& stats) = 0;

    virtual void sanityCheck(const Storage& storage) const = 0;
};

/// \brief Generic SPH solver, evaluating equations for each particle separately.
///
/// Contrary to \ref SymmetricSolver, this solver computes the derivatives for each particle separately,
/// meaning each particle pair is evaluated twice (or not at all, of course). This leads to generally slower
/// evaluation, as we have to compute twice as much interactions. However, the solver is more versatile and
/// can solve equations with asymmetric derivatives. This allows to use helper terms (like strain rate
/// correction tensor) that require two passes over the particle neighbours; the derivative is completed after
/// the neighbours are summed up, as the list of neighbours contains ALL of the neighbouring particles (as
/// opposed to SymmetricSolver, where the list of neighbours only contains the particles with lower smoothing
/// length).
///
/// Another benefit is lower memory overhead (all threads can write into the same buffers) and
/// generally higher CPU usage.
class AsymmetricSolver : public IAsymmetricSolver {
protected:
    /// Holds all derivatives (shared for all threads)
    DerivativeHolder derivatives;

    AutoPtr<IBoundaryCondition> bc;

    struct ThreadData {
        /// Cached array of neighbours, to avoid allocation every step
        Array<NeighbourRecord> neighs;

        /// Indices of real neighbours
        Array<Size> idxs;

        /// Cached array of gradients
        Array<Vector> grads;
    };

    ThreadLocal<ThreadData> threadData;

public:
    AsymmetricSolver(IScheduler& scheduler, const RunSettings& settings, const EquationHolder& eqs);

    AsymmetricSolver(IScheduler& scheduler,
        const RunSettings& settings,
        const EquationHolder& eqs,
        AutoPtr<IBoundaryCondition>&& bc);

    ~AsymmetricSolver();

protected:
    virtual void beforeLoop(Storage& storage, Statistics& stats) override;

    virtual void loop(Storage& storage, Statistics& stats) override;

    virtual void afterLoop(Storage& storage, Statistics& stats) override;

    virtual void sanityCheck(const Storage& storage) const override;
};

NAMESPACE_SPH_END
