#include "sph/initial/Distribution.h"
#include "math/Integrator.h"
#include "math/Morton.h"
#include "math/rng/VectorRng.h"
#include "objects/finders/UniformGrid.h"
#include "objects/geometry/Domain.h"
#include "objects/wrappers/Optional.h"
#include "quantities/Storage.h"
#include "sph/boundary/Boundary.h"
#include "system/Profiler.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// RandomDistribution implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

RandomDistribution::RandomDistribution(AutoPtr<IRng>&& rng)
    : rng(std::move(rng)) {}

RandomDistribution::RandomDistribution(const Size seed)
    : rng(makeRng<UniformRng>(seed)) {}

Array<Vector> RandomDistribution::generate(const Size n, const IDomain& domain) const {
    const Box bounds = domain.getBoundingBox();
    VectorRng<IRng&> boxRng(*rng);
    Array<Vector> vecs(0, n);
    // use homogeneous smoothing lenghs regardless of actual spatial variability of particle concentration
    const Float volume = domain.getVolume();
    const Float h = root<3>(volume / n);
    Size found = 0;
    for (Size i = 0; i < 1e5 * n && found < n; ++i) {
        Vector w = boxRng() * bounds.size() + bounds.lower();
        w[H] = h;
        if (domain.contains(w)) {
            vecs.push(w);
            ++found;
        }
    }
    return vecs;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CubicPacking implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

Array<Vector> CubicPacking::generate(const Size n, const IDomain& domain) const {
    PROFILE_SCOPE("CubicPacking::generate")
    ASSERT(n > 0);
    const Float volume = domain.getVolume();
    const Float particleDensity = Float(n) / volume;

    // interparticle distance based on density
    const Float h = 1._f / root<3>(particleDensity);
    ASSERT(isReal(h));

    const Box boundingBox = domain.getBoundingBox();
    const Vector step(h);
    const Box box(boundingBox.lower() + 0.5_f * step, boundingBox.upper());
    Array<Vector> vecs;
    box.iterate(step, [&vecs, &domain, h](Vector&& v) {
        if (domain.contains(v)) {
            v[H] = h;
            vecs.push(std::move(v));
        }
    });
    return vecs;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// HexagonalPacking implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

HexagonalPacking::HexagonalPacking(const Flags<Options> f)
    : flags(f) {}

Array<Vector> HexagonalPacking::generate(const Size n, const IDomain& domain) const {
    PROFILE_SCOPE("HexagonalPacking::generate")
    ASSERT(n > 0);
    const Float volume = domain.getVolume();
    const Float particleDensity = Float(n) / volume;

    // interparticle distance based on density
    const Float h = 1._f / root<3>(particleDensity);
    const Float dx = (flags.has(Options::SPH5_COMPATIBILITY) ? 1._f : 1.1_f) * h;
    const Float dy = sqrt(3._f) * 0.5_f * dx;
    const Float dz = sqrt(6._f) / 3._f * dx;

    const Box boundingBox = domain.getBoundingBox();
    const Vector step(dx, dy, dz);
    const Box box = flags.has(Options::SPH5_COMPATIBILITY)
                        ? boundingBox
                        : Box(boundingBox.lower() + 0.5_f * step, boundingBox.upper());
    Array<Vector> vecs;
    const Float deltaX = 0.5_f * dx;
    const Float deltaY = sqrt(3._f) / 6._f * dx;
    box.iterateWithIndices(step, [&](Indices&& idxs, Vector&& v) {
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
        if (domain.contains(v)) {
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DiehlEtAlDistribution implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

DiehlDistribution::DiehlDistribution(const DiehlDistribution::DensityFunc& particleDensity,
    const Float maxDifference,
    const Size numOfIters,
    const Float strength,
    const Float small)
    : particleDensity(particleDensity)
    , maxDifference(maxDifference)
    , numOfIters(numOfIters)
    , strength(strength)
    , small(small) {}

namespace {
class ForwardingDomain : public IDomain {
private:
    const IDomain& domain;

public:
    explicit ForwardingDomain(const IDomain& domain)
        : IDomain(domain.getCenter())
        , domain(domain) {}

    virtual Box getBoundingBox() const override {
        return domain.getBoundingBox();
    }

    virtual Float getVolume() const override {
        return domain.getVolume();
    }

    virtual bool contains(const Vector& v) const override {
        return domain.contains(v);
    }

    virtual void getSubset(ArrayView<const Vector> vs,
        Array<Size>& output,
        const SubsetType type) const override {
        return domain.getSubset(vs, output, type);
    }

    virtual void getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const override {
        domain.getDistanceToBoundary(vs, distances);
    }

    virtual void project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices = NOTHING) const override {
        domain.project(vs, indices);
    }
    virtual void addGhosts(ArrayView<const Vector> vs,
        Array<Ghost>& ghosts,
        const Float radius = 2._f,
        const Float eps = 0.05_f) const override {
        domain.addGhosts(vs, ghosts, radius, eps);
    }
};
} // namespace

/// Renormalizes particle density so that integral matches expected particle count.
///
/// Uses iterative approach, finding normalization coefficient until the different between the expected and
/// the final number of particles is less than the error.
/// \param domain in which the particles are created
/// \param n Input/output parameter - Takes the expected number of particles and returns the final number.
/// \param error Maximum difference between the expected and the final number of particles.
/// \param density Input (unnormalized) particle density
/// \return Functor representing the renormalized density
template <typename TDensity>
static auto renormalizeDensity(const IDomain& domain, Size& n, const Size error, TDensity& density) {
    Float multiplier = 1._f;
    auto actDensity = [&domain, &density, &multiplier](const Vector& v) {
        if (domain.contains(v)) {
            return multiplier * density(v);
        } else {
            return 0._f;
        }
    };

    Integrator<HaltonQrng> mc(makeAuto<ForwardingDomain>(domain));
    Size cnt = 0;
    Float particleCnt;
    for (particleCnt = mc.integrate(actDensity); abs(particleCnt - n) > error;) {
        const Float ratio = n / max(particleCnt, 1._f);
        ASSERT(ratio > EPS);
        multiplier *= ratio;
        particleCnt = mc.integrate(actDensity);
        if (cnt++ > 100) { // break potential infinite loop
            break;
        }
    }
    n = particleCnt;
    // create a different functor that captures multiplier by value, so we can return it from the function
    return [&domain, &density, multiplier](const Vector& v) {
        if (domain.contains(v)) {
            return multiplier * density(v);
        } else {
            return 0._f;
        }
    };
}

/// Generate the initial positions in Diehl's distribution.
/// \param domain Domain in which the particles are created
/// \param N Number of particles to create
/// \param density Functor describing the particle density (concentration).
template <typename TDensity>
static Storage generateInitial(const IDomain& domain, const Size N, TDensity&& density) {
    Box boundingBox = domain.getBoundingBox();
    VectorPdfRng<HaltonQrng> rng(boundingBox, density);

    Array<Vector> r(0, N);
    for (Size i = 0; i < N; ++i) {
        Vector pos = rng();
        const Float n = density(pos);
        pos[H] = 1._f / root<3>(n);
        ASSERT(isReal(pos));
        r.push(pos);
    }

    // create a dummy storage so that we can use the functionality of GhostParticles
    Storage storage;
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::ZERO, std::move(r));
    return storage;
}

Array<Vector> DiehlDistribution::generate(const Size expectedN, const IDomain& domain) const {
    Size N = expectedN;
    auto actDensity = renormalizeDensity(domain, N, maxDifference, particleDensity);
    ASSERT(abs(int(N) - int(expectedN)) <= maxDifference);

    // generate initial particle positions
    Storage storage = generateInitial(domain, N, actDensity);
    Array<Vector>& r = storage.getValue<Vector>(QuantityId::POSITION);

    GhostParticles bc(makeAuto<ForwardingDomain>(domain), 2._f, EPS);

    UniformGridFinder finder;
    ThreadPool& pool = ThreadPool::getGlobalInstance();
    ThreadLocal<Array<NeighbourRecord>> neighs(pool);
    finder.build(r);

    const Float correction = strength / (1.f + small);
    // radius of search, does not have to be equal to radius of used SPH kernel
    const Float kernelRadius = 2._f;

    Array<Vector> deltas(N);
    for (Size k = 0; k < numOfIters; ++k) {
        // gradually decrease the strength of particle dislocation
        const Float converg = 1._f / sqrt(Float(k + 1));

        // add ghost particles
        bc.initialize(storage);

        // reconstruct finder to allow for variable topology of particles (we need to reset the internal
        // arrayview as we added ghosts)
        finder.build(r);

        // clean up the previous displacements
        deltas.fill(Vector(0._f));

        auto lambda = [&](const Size i, Array<NeighbourRecord>& neighsTl) {
            const Float rhoi = actDensity(r[i]); // average particle density
            // average interparticle distance at given point
            const Float neighbourRadius = kernelRadius / root<3>(rhoi);
            finder.findAll(i, neighbourRadius, neighsTl);

            for (Size j = 0; j < neighsTl.size(); ++j) {
                const Size k = neighsTl[j].index;
                const Vector diff = r[k] - r[i];
                const Float lengthSqr = getSqrLength(diff);
                // for ghost particles, just copy the density (density outside the domain is always 0)
                const Float rhok = (k >= N) ? rhoi : actDensity(r[k]);
                // average kernel radius to allow for the gradient of particle density
                const Float h = kernelRadius * (0.5f / root<3>(rhoi) + 0.5f / root<3>(rhok));
                if (lengthSqr > h * h || lengthSqr == 0) {
                    continue;
                }
                const Float hSqrInv = 1.f / (h * h);
                const Float length = getLength(diff);
                ASSERT(length != 0._f);
                const Vector diffUnit = diff / length;
                const Float t =
                    converg * h * (strength / (small + getSqrLength(diff) * hSqrInv) - correction);
                deltas[i] += diffUnit * min(t, h); // clamp the displacement to particle distance
                ASSERT(isReal(deltas[i]));
            }
            deltas[i][H] = 0._f; // do not affect smoothing lengths
        };
        parallelFor(pool, neighs, 0, N, 100, lambda);

        // apply the computed displacements; note that r might be larger than deltas due to ghost particles -
        // we don't need to move them
        for (Size i = 0; i < deltas.size(); ++i) {
            r[i] -= deltas[i];
        }

        // remove the ghosts
        bc.finalize(storage);

        // project particles outside of the domain to the boundary
        // (there shouldn't be any, but it may happen for large strengths / weird boundaries)
        domain.project(r);
    }

#ifdef SPH_DEBUG
    for (Size i = 0; i < N; ++i) {
        ASSERT(isReal(r[i]) && r[i][H] > 1.e-20_f);
    }
#endif
    // extract the buffers from the storage
    Array<Vector> positions = std::move(storage.getValue<Vector>(QuantityId::POSITION));
    return positions;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// LinearDistribution implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

Array<Vector> LinearDistribution::generate(const Size n, const IDomain& domain) const {
    const Float center = domain.getCenter()[X];
    const Float radius = 0.5_f * domain.getBoundingBox().size()[X];
    Array<Vector> vs(0, n);
    const Float dx = 2._f * radius / (n - 1);
    for (Size i = 0; i < n; ++i) {
        const Float x = center - radius + (2._f * radius * i) / (n - 1);
        vs.push(Vector(x, 0._f, 0._f, 1.5_f * dx)); // smoothing length = interparticle distance
    }
    return vs;
}

NAMESPACE_SPH_END
