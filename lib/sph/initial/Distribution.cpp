#include "sph/initial/Distribution.h"
#include "math/Morton.h"
#include "math/rng/VectorRng.h"
#include "geometry/Domain.h"
#include "math/Integrator.h"
#include "objects/finders/Voxel.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

Array<Vector> RandomDistribution::generate(const Size n, const Abstract::Domain& domain) const {
    const Vector center(domain.getCenter());
    const Vector radius(domain.getBoundingRadius());
    const Box bounds(center - radius, center + radius);

    VectorPdfRng<HaltonQrng> boxRng(bounds);
    Array<Vector> vecs(0, n);
    // use homogeneous smoothing lenghs regardless of actual spatial variability of particle concentration
    const Float volume = domain.getVolume();
    const Float h = root<3>(volume / n);
    Size found = 0;
    for (Size i = 0; i < 1e5 * n && found < n; ++i) {
        Vector w = boxRng();
        w[H] = h;
        if (domain.isInside(w)) {
            vecs.push(w);
            ++found;
        }
    }
    return vecs;
}

Array<Vector> CubicPacking::generate(const Size n, const Abstract::Domain& domain) const {
    PROFILE_SCOPE("CubicPacking::generate")
    ASSERT(n > 0);
    const Float volume = domain.getVolume();
    const Float particleDensity = Float(n) / volume;

    // interparticle distance based on density
    const Float h = 1._f / root<3>(particleDensity);

    const Vector center(domain.getCenter());
    const Vector radius(domain.getBoundingRadius() + h);
    const Box box(center - radius, center + radius);

    Array<Vector> vecs(0, 2 * n); /// \todo better estimate of how many we need to allocate, or
                                  /// reallocation like std::vector
    box.iterate(Vector(h, h, h), [&vecs, &domain, h](Vector&& v) {
        if (domain.isInside(v)) {
            v[H] = h;
            vecs.push(std::move(v));
        }
    });
    return vecs;
}

HexagonalPacking::HexagonalPacking(const Flags<Options> flags)
    : flags(flags) {}

Array<Vector> HexagonalPacking::generate(const Size n, const Abstract::Domain& domain) const {
    PROFILE_SCOPE("HexagonalPacking::generate")
    ASSERT(n > 0);
    const Float volume = domain.getVolume();
    const Float particleDensity = Float(n) / volume;

    // interparticle distance based on density
    const Float h = 1._f / root<3>(particleDensity);
    const Float dx = 1.1_f * h;
    const Float dy = sqrt(3._f) * 0.5_f * dx;
    const Float dz = sqrt(6._f) / 3._f * dx;

    const Vector center(domain.getCenter());
    const Vector radius(domain.getBoundingRadius());
    const Box box(center - radius, center + radius);

    Array<Vector> vecs;
    const Float deltaX = 0.5_f * dx;
    const Float deltaY = sqrt(3._f) / 6._f * dx;
    Float lastY = 0._f;
    box.iterateWithIndices(
        Vector(dx, dy, dz), [&lastY, deltaX, deltaY, &vecs, &domain, h](Indices&& idxs, Vector&& v) {
            if (idxs[2] % 2 == 0) {
                if (idxs[1] % 2 == 1) {
                    v[X] += deltaX;
                }
            } else {
                if (idxs[1] % 2 == 0) {
                    v[X] += deltaX;
                }
                v[Y] += deltaY;
            }
            if (domain.isInside(v)) {
                v[H] = h;
                vecs.push(std::move(v));
            }
        });
    if (flags.has(Options::SORTED)) {
        // sort by Morton code
        std::sort(vecs.begin(), vecs.end(), [&box](Vector& v1, Vector& v2) {
            // compute relative coordinates in bounding box
            const Vector vr1 = (v1 - box.lower()) / box.size();
            const Vector vr2 = (v2 - box.lower()) / box.size();
            return morton(vr1) > morton(vr2);
        });
    }
    if (flags.has(Options::CENTER)) {
        ASSERT(!vecs.empty());
        Vector com(0._f);
        for (const Vector& v : vecs) {
            com += v;
        }
        com /= vecs.size();
        // match center of mass to center of domain
        const Vector delta = domain.getCenter() - com;
        for (Vector& v : vecs) {
            const Float h = v[H];
            v += delta;
            v[H] = h;
        }
    }
    return vecs;
}


DiehlEtAlDistribution::DiehlEtAlDistribution(const DiehlEtAlDistribution::DensityFunc& particleDensity,
    const Float error,
    const Size numOfIters,
    const Float strength,
    const Float small)
    : particleDensity(particleDensity)
    , error(error)
    , numOfIters(numOfIters)
    , strength(strength)
    , small(small) {}

Array<Vector> DiehlEtAlDistribution::generate(const Size n, const Abstract::Domain& domain) const {
    // Renormalize particle density so that integral matches expected particle count
    Float multiplier = 1._f;
    auto actDensity = [this, &multiplier](const Vector& v) { return multiplier * particleDensity(v); };

    Integrator<HaltonQrng> mc(domain);
    Size cnt = 0;
    Float particleCnt;
    for (particleCnt = mc.integrate(actDensity); abs(particleCnt - n) > error;) {
        const Float ratio = n / max(particleCnt, 1._f);
        multiplier *= ratio;
        particleCnt = mc.integrate(actDensity);
        if (cnt++ > 100) { // break potential infinite loop
            break;
        }
    }
    const Size N = Size(particleCnt); // final particle count of the target

    Box boundingBox(domain.getCenter() - Vector(domain.getBoundingRadius()),
        domain.getCenter() + Vector(domain.getBoundingRadius()));
    VectorPdfRng<HaltonQrng> rng(boundingBox, actDensity);

    // generate initial particle positions
    Array<Vector> vecs;
    for (Size i = 0; i < N; ++i) {
        Vector v = rng();
        const Float n = actDensity(v);
        const Float h = 1._f / root<3>(n);
        v[H] = h;
        vecs.push(v);
    }

    VoxelFinder finder;
    Array<NeighbourRecord> neighs;
    finder.build(vecs);

    const Float correction = strength / (1.f + small);
    // radius of search, does not have to be equal to radius of used SPH kernel
    const Float kernelRadius = 2._f;

    for (Size k = 0; k < numOfIters; ++k) {
        // gradually decrease the strength of particle dislocation
        const Float converg = 1._f / sqrt(Float(k + 1));

        // reconstruct finder to allow for variable topology of particles
        finder.rebuild();

        // for all particles ...
        for (Size i = 0; i < N; ++i) {
            Vector delta(0._f);
            const Float n = actDensity(vecs[i]); // average particle density
            // average interparticle distance at given point
            const Float neighbourRadius = kernelRadius / root<3>(n);
            finder.findNeighbours(i, neighbourRadius, neighs, EMPTY_FLAGS, 1.e-3_f);

            /// \todo find a way to add ghosts
            // create ghosts for particles near the boundary
            // Vector<float, d> mirror;
            // if (vecs[i].getSqrLength() > radius - neighbourRadius) {
            //    mirror = vecs[i].sphericalInversion(Vector<float, d>(0.f), radius);
            //    neighbours.push(&mirror);
            //}
            for (Size j = 0; j < neighs.size(); ++j) {
                const Size k = neighs[j].index;
                const Vector diff = vecs[k] - vecs[i];
                const Float lengthSqr = getSqrLength(diff);
                // average kernel radius to allow for the gradient of particle density
                const Float h = kernelRadius *
                                (0.5f / root<3>(actDensity(vecs[i])) + 0.5f / root<3>(actDensity(vecs[k])));
                if (lengthSqr > h * h || lengthSqr == 0) {
                    continue;
                }
                const Float hSqrInv = 1.f / (h * h);
                const Float length = getLength(diff);
                ASSERT(length != 0._f);
                const Vector diffUnit = diff / length;
                const Float t =
                    converg * h * (strength / (small + getSqrLength(diff) * hSqrInv) - correction);
                delta += diffUnit * min(t, h); // clamp the dislocation to particle distance
            }
            vecs[i] = vecs[i] - delta;
        }
        // project particles outside of the domain to the boundary
        domain.project(vecs);
    }

    return vecs;
}

Array<Vector> LinearDistribution::generate(const Size n, const Abstract::Domain& domain) const {
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

NAMESPACE_SPH_END
