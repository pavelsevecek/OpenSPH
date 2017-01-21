#include "sph/initial//Distribution.h"
#include "math/Morton.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

Array<Vector> RandomDistribution::generate(const Size n, const Abstract::Domain& domain) const {
    const Vector center(domain.getCenter());
    const Vector radius(domain.getBoundingRadius());
    const Box bounds(center - radius, center + radius);

    auto boxRng = makeVectorPdfRng(
        bounds, HaltonQrng(), [](const Vector&) { return 1._f; }, [](const Vector&) { return 1._f; });
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
    const Float dx = 1._f * h;
    const Float dy = sqrt(3._f) * 0.5_f * dx;
    const Float dz = sqrt(6._f) / 3._f * dx;

    const Vector center(domain.getCenter());
    const Vector radius(domain.getBoundingRadius());
    const Box box(center - radius, center + radius);

    /// \todo generalize to 1 and 2 dim
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

/*
NonUniformDistribution::NonUniformDistribution(const NonUniformDistribution::DensityFunc& particleDensity, const Float error)
    : particleDensity(particleDensity)
    , error(error) {}*/

/*Array<Vector> NonUniformDistribution::generate(const Size n, const Abstract::Domain& domain) const {
    // Renormalize particle density so that integral matches expected particle count
    Float multiplier = 1._f;
    auto actDensity = [this, &multiplier](const Vector& v) { return multiplier * particleDensity(v); };

    Integrator<HaltonQrng> mc(domain);
    Size cnt = 0;
    Float particleCnt;
    for (particleCnt = mc.integrate(actDensity); abs(particleCnt - n) > error;) {
        const Float ratio = n / max(particleCnt, 1.f);
        multiplier *= ratio;
        particleCount = mc.integrate(lambda);
        if (cnt++ > 100) { // break potential infinite loop
            break;
        }
    }
    const Size N = Size(particleCount); // final particle count of the target

    HaltonQrng rng

    // generate initial particle positions
    vecs.forAll([&rng](Vector<Length<T>, d>& v) { v = 1._m * rng.rand(); });

    KdTree tree(vecs);           // TODO: makeKdTree
    Array<int> neighbours(0, N); // empty array of maximal size N
    const float correction = strength / (1.f + SMALL);


    float average = 0.f;
    float averageSqr = 0.f;
    int count;
    for (int k = 0; k < iterNum; ++k) {
        // gradually decrease the strength of particle dislocation
        const float converg = 1.f / sqrt(float(k + 1));
        std::cout << k << std::endl;

        // statistics
        average = 0.f;
        averageSqr = 0.f;
        count = 0;
        // reconstruct K-d tree to allow for variable topology of particles
        tree.rebuildTree();

        // for all particles ...
        for (int i = 0; i < N; ++i) {
            Vector<Length<T>, d> delta(0._m);
            const NumberDensity<T> n = lambda(vecs[i]); // average particle density
            // average interparticle distance at given point
            const Length<T> neighbourRadius = KERNEL_RADIUS / root<d>(n);
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
                const float h =
                    KERNEL_RADIUS * (0.5f / root<d>(lambda(vecs[i])) + 0.5f / root<d>(lambda(vecs[k])));
                if (lengthSqr > h * h || lengthSqr == 0) {
                    continue;
                }
                const float hSqrInv = 1.f / (h * h);
                const float length = diff.getLength();
                average += length / h;
                averageSqr += sqr(length) / sqr(h);
                count++;
                const Vector diffUnit = diff / length;
                const float t =
                    converg * h * (strength / (SMALL + diff.getSqrLength() * hSqrInv) - correction);
                if (MOVE_PARTICLES) {
                    delta += diffUnit * min(t, h); // clamp the dislocation to particle distance
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
              << "average = " << average << ", variance = " << averageSqr - average * average << std::endl;


    return N;
}*/

NAMESPACE_SPH_END
