#pragma once

/// Generating initial conditions of SPH particles
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Box.h"
#include "math/Integrator.h"
#include "math/rng/VectorRng.h"
#include "objects/containers/Array.h"
#include "objects/finders/KdTree.h"

NAMESPACE_SPH_BEGIN


namespace Abstract {
    class Distribution : public Polymorphic {
    public:
        /// Base class for generating vertices with specific distribution. Also generates corresponding
        /// smoothing lengths and save them as fourth component of the vector.
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
    };

    HexagonalPacking(const Flags<Options> flags = Options::CENTER);

    virtual Array<Vector> generate(const Size n, const Abstract::Domain& domain) const override;

private:
    Flags<Options> flags;
};


/// Distribution with given particle density. Particles are placed using algorithm by Diehl et al. (2012).
/*class NonUniformDistribution : public Abstract::Distribution {
private:
    using DensityFunc = std::function<Float(const Vector& position)>;
    DensityFunc particleDensity;
    Float error;

public:
    /// Constructs a distribution using function returning expected particle density at given position.
    /// Function does not have to be normalized, only a relative number of particles at different places is
    /// relevant.
    /// \param error Allowed relative error in number of generated particles. Lower value generates number of
    ///              particles closer to required value, but takes longer to compute.
    NonUniformDistribution(const DensityFunc& particleDensity, const Float error);

    /// Returns generated particle distribution. Smoothing lengths correspond to particle density given in the
    /// constructor (as h ~ n^(-1/3) )
    virtual Array<Vector> generate(const Size n, const Abstract::Domain& domain) const override;
};*/


/// Generates particles uniformly on a line in x direction, for testing purposes. Uses only center and radius
/// of the domain.
class LinearDistribution : public Abstract::Distribution {
public:
    virtual Array<Vector> generate(const Size n, const Abstract::Domain& domain) const override {
        const Float center = domain.getCenter()[X];
        const Float radius = domain.getBoundingRadius();
        Array<Vector> vs(0, n);
        const Float dx = 2._f * radius / (n - 1);
        for (Size i = 0; i < n; ++i) {
            const Float x = center - radius + (2._f * radius * i) / (n - 1);
            vs.push(Vector(x, 0._f, 0._f, 1.5_f * dx)); // smoothing length = interparticle distance
        }
        return vs;
    }
};

NAMESPACE_SPH_END
