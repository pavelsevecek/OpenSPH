#pragma once

/// \file Distribution.h
/// \brief Filling spatial domain with SPH particles
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "geometry/Box.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Flags.h"

NAMESPACE_SPH_BEGIN


namespace Abstract {
    class Domain;

    /// Base class for generating vertices with specific distribution. Also generates corresponding
    /// smoothing lengths and save them as fourth component of the vector.
    class Distribution : public Polymorphic {
    public:
        /// \param n Expected number of generated vertices.
        /// \param domain Computational domain where the vertices are distributed
        /// \return Output array of vertices. The total number of vertices can slightly differ from n.
        /// \note This method is expected to be called once at the beginning of the run, so we can
        ///       return allocated array without worrying about performance costs here.
        virtual Array<Vector> generate(const Size n, const Domain& domain) const = 0;
    };
}

/// Generating random positions withing the domain.
class RandomDistribution : public Abstract::Distribution {
public:
    virtual Array<Vector> generate(const Size n, const Abstract::Domain& domain) const override;
};


/// Cubic close packing
class CubicPacking : public Abstract::Distribution {
public:
    virtual Array<Vector> generate(const Size n, const Abstract::Domain& domain) const override;
};


/// Hexagonal close packing
class HexagonalPacking : public Abstract::Distribution {
public:
    enum class Options {
        SORTED = 1 << 0, ///< Particles are sorted using its Morton code, particles close in space are also
                         /// close in  memory. By default, particles are sorted along x axis, secondary
                         /// along y and z axes.
        CENTER = 1 << 1, ///< Move particle lattice so that their center of mass matches center of domain.
                         /// This assumes all particles have the same mass. Note that with this options,
                         /// generated particles does not have to be strictly inside given domain.
        SPH5_COMPATIBILITY = 1 << 2 ///< Generates the grid to match code SPH5 for 1-1 comparison. Not that
                                    /// this will generate significantly more particles than requested
                                    /// (approximately by factor 1.4).
    };

    HexagonalPacking(const Flags<Options> flags = Options::CENTER);

    virtual Array<Vector> generate(const Size n, const Abstract::Domain& domain) const override;

private:
    Flags<Options> flags;
};


/// Distribution with given particle density. Particles are placed using algorithm by Diehl et al. (2012).
class DiehlEtAlDistribution : public Abstract::Distribution {
private:
    using DensityFunc = std::function<Float(const Vector& position)>;
    DensityFunc particleDensity;
    Float error;
    Size numOfIters;
    Float strength;
    Float small;

public:
    /// Constructs a distribution using function returning expected particle density at given position.
    /// Function does not have to be normalized, only a relative number of particles at different places is
    /// relevant.
    /// \param error Allowed relative error in number of generated particles. Lower value generates number of
    ///              particles closer to required value, but takes longer to compute.
    /// \param numOfIters Number of iterations. For zero, distribution of particles is simply random, higher
    ///                   values lead to more evenly distributed particles (less discrepancy), but also take
    ///                   longer to compute.
    /// \param strenth Magnitude of repulsive force between particles that iteratively moves the to their
    ///                final locations. Larger values mean faster convergence but less stable particle grid.
    /// \param small Normalization value to prevent division by zero for overlapping particles. Keep default,
    ///              only for testing.
    DiehlEtAlDistribution(const DensityFunc& particleDensity,
        const Float error = 10,
        const Size numOfIters = 50,
        const Float strenth = 0.1_f,
        const Float small = 0.1_f);

    /// Returns generated particle distribution. Smoothing lengths correspond to particle density given in the
    /// constructor (as h ~ n^(-1/3) )
    virtual Array<Vector> generate(const Size n, const Abstract::Domain& domain) const override;
};

/// Generates particles uniformly on a line in x direction, for testing purposes. Uses only center and radius
/// of the domain.
class LinearDistribution : public Abstract::Distribution {
public:
    virtual Array<Vector> generate(const Size n, const Abstract::Domain& domain) const override;
};

NAMESPACE_SPH_END
