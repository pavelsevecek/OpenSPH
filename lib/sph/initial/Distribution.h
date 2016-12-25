#pragma once

/// Generating initial conditions of SPH particles
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Bounds.h"
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
        virtual Array<Vector> generate(const int n, const Domain& domain) const = 0;
    };
}

/// Generating random positions withing the domain.
class RandomDistribution : public Abstract::Distribution {
public:
    virtual Array<Vector> generate(const int n, const Abstract::Domain& domain) const override;
};


/// Cubic close packing
class CubicPacking : public Abstract::Distribution {
public:
    virtual Array<Vector> generate(const int n, const Abstract::Domain& domain) const override;
};


/// Hexagonal close packing
class HexagonalPacking : public Abstract::Distribution {
public:
    virtual Array<Vector> generate(const int n, const Abstract::Domain& domain) const override;
};


/// Generates particles uniformly on a line in x direction, for testing purposes. Uses only center and radius
/// of the domain.
class LinearDistribution : public Abstract::Distribution {
public:
    virtual Array<Vector> generate(const int n, const Abstract::Domain& domain) const override {
        const Float center = domain.getCenter()[X];
        const Float radius = domain.getBoundingRadius();
        Array<Vector> vs(0, n);
        const Float dx = (2._f * radius - EPS) / Float(n - 1);
        for (Float x = center - radius; x <= center + radius; x += dx) {
            vs.push(Vector(x, 0._f, 0._f, 1.5_f * dx)); // smoothing length = interparticle distance
        }
        return vs;
    }
};
/*
template <typename T, int d>
class Nonuniform : public AbstractDistribution {
private:
    const float KERNEL_RADIUS = 1.7f;
    const float SMALL         = 0.1f;

    const float NEIGHBOUR_ERROR = 0.2f;
    const bool UNIFORM          = false;
    const bool MOVE_PARTICLES   = true;

    const int dim        = 2;
    const float strength = 0.1f;

    const float allowedError = 20.f;
    float drop               = 100.f;
    const int iterNum        = 50;

    const Length<T> projectRadius;
    const int projectCnt;
    const Angle<T> angle;


public:
    Nonuniform(const T projectRadius, const int projectCnt, const Angle<T> angle)
        : projectRadius(projectRadius)
        , projectCnt(projectCnt)
        , angle(angle) {}

    virtual int generateSphere(const int n, const T radius, Array<Vector>& vecs) override {

        const Volume<T> projectVolume(Math::sphereVolume(projectRadius));
        const NumberDensity<T> projectDensity = T(projectCnt) / projectVolume;
        const Volume<T> targetVolume(Math::sphereVolume(radius));

        const auto center = Vector<Length<T>, d>::spherical(radius, angle.value(Units::SI<T>));

        // prescribed particle density
        // parameter drop is determined by total number of particles in target
        auto lambda = [&](const Vector<Length<T>, d>& v) {
            return projectDensity / (1.f + drop * (v - center).getSqrLength() / (radius * radius));
        };

        Integrator<float, d> mc(radius.value());
        int cnt = 0;
        float particleCount;
        for (particleCount = mc.integrate(lambda); Math::abs(particleCount - n) > allowedError;) {
            const float ratio = particleCount / n;
            drop *= ratio;
            std::cout << "ratio = " << ratio << "  drop = " << drop << " particle count  " <<
particleCount
                      << std::endl;
            particleCount = mc.integrate(lambda);
            if (cnt++ > 100) { // break potential infinite loop
                break;
            }
        }
        const int N = Math::round(particleCount); // final particle count of the target

        HaltonQrng<T> halton;
        auto rng = makeSphericalRng(Vector(0.f), radius.value(), halton, lambda);

        // generate initial particle positions
        vecs.forAll([&rng](Vector<Length<T>, d>& v) { v = 1._m * rng.rand(); });

        KdTree tree(vecs);     // TODO: makeKdTree
        Array<int> neighbours(0, N); // empty array of maximal size N
        const float correction = strength / (1.f + SMALL);


        float average    = 0.f;
        float averageSqr = 0.f;
        int count;
        for (int k = 0; k < iterNum; ++k) {
            // gradually decrease the strength of particle dislocation
            const float converg = 1.f / sqrt(float(k + 1));
            std::cout << k << std::endl;

            // statistics
            average    = 0.f;
            averageSqr = 0.f;
            count      = 0;
            // reconstruct K-d tree to allow for variable topology of particles
            tree.rebuildTree();

            // for all particles ...
            for (int i = 0; i < N; ++i) {
                Vector<Length<T>, d> delta(0._m);
                const NumberDensity<T> n = lambda(vecs[i]); // average particle density
                // average interparticle distance at given point
                const Length<T> neighbourRadius = KERNEL_RADIUS / Math::root<d>(n);
                neighbours.resize(0);
                tree.findNeighbours(vecs[i], neighbourRadius, neighbours, NEIGHBOUR_ERROR);

                // TODO: find a way to add ghosts
                // create ghosts for particles near the boundary
                // Vector<float, d> mirror;
                // if (vecs[i].getSqrLength() > radius - neighbourRadius) {
                //    mirror = vecs[i].sphericalInversion(Vector<float, d>(0.f), radius);
                //    neighbours.push(&mirror);
                //}
                for (int j = 0; j < neighbours.getSize(); ++j) {
                    const int k = neighbours[j];
                    const Vector diff = vecs[k] - vecs[i];
                    const float lengthSqr = diff.getSqrLength();
                    // average kernel radius to allow for the gradient of particle density
                    const float h = KERNEL_RADIUS * (0.5f / Math::root<d>(lambda(vecs[i])) +
                                                     0.5f / Math::root<d>(lambda(vecs[k])));
                    if (lengthSqr > h * h || lengthSqr == 0) {
                        continue;
                    }
                    const float hSqrInv = 1.f / (h * h);
                    const float length  = diff.getLength();
                    average += length / h;
                    averageSqr += Math::sqr(length) / Math::sqr(h);
                    count++;
                    const Vector diffUnit = diff / length;
                    const float t =
                        converg * h * (strength / (SMALL + diff.getSqrLength() * hSqrInv) - correction);
                    if (MOVE_PARTICLES) {
                        delta += diffUnit * Math::min(t, h); // clamp the dislocation to particle distance
                    }
                }
                vecs[i] = vecs[i] - delta;
                // project particles outside of the sphere to the boundary
                if (vecs[i].getSqrLength() > radius * radius) {
                    vecs[i] = radius * vecs[i].getNormalized();
                }
            }
            average /= count;
            averageSqr /= count;
        }
        // print statistics
        std::cout << std::endl
                  << "average = " << average << ", variance = " << averageSqr - average * average
                  << std::endl;


        return N;
    }
};
*/


NAMESPACE_SPH_END
