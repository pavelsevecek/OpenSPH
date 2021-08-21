#include "sph/initial/Distribution.h"
#include "math/Functional.h"
#include "math/Morton.h"
#include "math/rng/VectorRng.h"
#include "objects/finders/UniformGrid.h"
#include "objects/geometry/Domain.h"
#include "objects/wrappers/Optional.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/boundary/Boundary.h"
#include "system/Profiler.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

//-----------------------------------------------------------------------------------------------------------
// RandomDistribution implementation
//-----------------------------------------------------------------------------------------------------------

RandomDistribution::RandomDistribution(AutoPtr<IRng>&& rng)
    : rng(std::move(rng)) {}

RandomDistribution::RandomDistribution(const Size seed)
    : rng(makeRng<UniformRng>(seed)) {}

Array<Vector> RandomDistribution::generate(IScheduler& UNUSED(scheduler),
    const Size n,
    const IDomain& domain) const {
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

//-----------------------------------------------------------------------------------------------------------
// StratifiedDistribution implementation
//-----------------------------------------------------------------------------------------------------------

static Float findStep(const Box& bounds, const Size n) {
    const Vector size = bounds.size();
    Float step = maxElement(size);
    Size particlesPerRegion = n;
    while (particlesPerRegion > 1000) {
        step /= 2;
        const Size numRegions = Size(ceil(size[X] / step) * ceil(size[Y] / step) * ceil(size[Z] / step));
        particlesPerRegion = n / numRegions;
    }
    return step;
}

StratifiedDistribution::StratifiedDistribution(const Size seed)
    : rng(makeRng<UniformRng>(seed)) {}

Array<Vector> StratifiedDistribution::generate(IScheduler& UNUSED(scheduler),
    const Size n,
    const IDomain& domain) const {
    VectorRng<IRng&> boxRng(*rng);
    Array<Vector> vecs(0, n);
    const Float volume = domain.getVolume();
    const Float h = root<3>(volume / n);
    Size found = 0;
    const Box bounds = domain.getBoundingBox();
    const Vector step(findStep(bounds, n));
    for (Size i = 0; i < 1e5 * n && found < n; ++i) {
        bounds.iterate(step, [h, &step, &vecs, &found, &domain, &boxRng](const Vector& r) {
            const Box box(r, r + step);
            Vector w = boxRng() * box.size() + box.lower();
            w[H] = h;
            if (domain.contains(w)) {
                vecs.push(w);
                ++found;
            }
        });
    }
    return vecs;
}

//-----------------------------------------------------------------------------------------------------------
// CubicPacking implementation
//-----------------------------------------------------------------------------------------------------------

Array<Vector> CubicPacking::generate(IScheduler& UNUSED(scheduler),
    const Size n,
    const IDomain& domain) const {
    PROFILE_SCOPE("CubicPacking::generate")
    SPH_ASSERT(n > 0);
    const Float volume = domain.getVolume();
    const Float particleDensity = Float(n) / volume;

    // interparticle distance based on density
    const Float h = 1._f / root<3>(particleDensity);
    SPH_ASSERT(isReal(h));

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

//-----------------------------------------------------------------------------------------------------------
// HexagonalPacking implementation
//-----------------------------------------------------------------------------------------------------------

HexagonalPacking::HexagonalPacking(const Flags<Options> flags)
    : flags(flags) {}

Array<Vector> HexagonalPacking::generate(IScheduler& UNUSED(scheduler),
    const Size n,
    const IDomain& domain) const {
    VERBOSE_LOG
    SPH_ASSERT(n > 0);
    const Float volume = domain.getVolume();
    const Float particleDensity = Float(n) / volume;

    // interparticle distance based on density
    const Float h = 1._f / root<3>(particleDensity);
    const Float dx = (flags.has(Options::SPH5_COMPATIBILITY) ? 1._f : 1.1_f) * h;
    const Float dy = sqrt(3._f) * 0.5_f * dx;
    const Float dz = sqrt(6._f) / 3._f * dx;

    const Box boundingBox = domain.getBoundingBox();
    SPH_ASSERT(boundingBox.volume() > 0._f && boundingBox.volume() < pow<3>(LARGE));
    const Vector step(dx, dy, dz);
    const Box box = flags.has(Options::SPH5_COMPATIBILITY)
                        ? boundingBox
                        : Box(boundingBox.lower() + 0.5_f * step, boundingBox.upper());
    Array<Vector> vecs;
    const Float deltaX = 0.5_f * dx;
    const Float deltaY = sqrt(3._f) / 6._f * dx;

    this->startProgress(n);

    std::atomic_bool shouldContinue{ true };
    box.iterateWithIndices(step, [&](Indices&& idxs, Vector&& v) {
        if (!shouldContinue) {
            return;
        }

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

            if (!this->tickProgress()) {
                shouldContinue = false;
            }
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
        SPH_ASSERT(!vecs.empty());
        Vector com(0._f);
        for (const Vector& v : vecs) {
            com += v;
        }
        com /= vecs.size();
        // match center of mass to center of domain
        Vector delta = domain.getCenter() - com;
        delta[H] = 0._f;
        for (Vector& v : vecs) {
            v += delta;
        }
    }
    return vecs;
}

//-----------------------------------------------------------------------------------------------------------
// DiehlDistribution implementation
//-----------------------------------------------------------------------------------------------------------

DiehlDistribution::DiehlDistribution(const DiehlParams& params)
    : params(params) {}

namespace {

class ForwardingDomain : public IDomain {
private:
    const IDomain& domain;

public:
    explicit ForwardingDomain(const IDomain& domain)
        : domain(domain) {}

    virtual Vector getCenter() const override {
        return domain.getCenter();
    }

    virtual Box getBoundingBox() const override {
        return domain.getBoundingBox();
    }

    virtual Float getVolume() const override {
        return domain.getVolume();
    }

    virtual Float getSurfaceArea() const override {
        return domain.getSurfaceArea();
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
    VERBOSE_LOG

    Float multiplier = n / domain.getVolume();
    auto actDensity = [&domain, &density, &multiplier](const Vector& v) {
        if (domain.contains(v)) {
            return multiplier * density(v);
        } else {
            return 0._f;
        }
    };

    Integrator<HaltonQrng> mc(domain);
    Size cnt = 0;
    Float particleCnt;
    for (particleCnt = mc.integrate(actDensity); abs(particleCnt - n) > error;) {
        const Float ratio = n / max(particleCnt, 1._f);
        SPH_ASSERT(ratio > EPS, ratio);
        multiplier *= ratio;
        particleCnt = mc.integrate(actDensity);
        if (cnt++ > 100) { // break potential infinite loop
            break;
        }
    }
    n = Size(particleCnt);
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
        SPH_ASSERT(isReal(pos));
        r.push(pos);
    }

    // create a dummy storage so that we can use the functionality of GhostParticles
    Storage storage;
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::ZERO, std::move(r));
    return storage;
}

Array<Vector> DiehlDistribution::generate(IScheduler& scheduler,
    const Size expectedN,
    const IDomain& domain) const {
    VERBOSE_LOG

    Size N = expectedN;
    auto actDensity = renormalizeDensity(domain, N, params.maxDifference, params.particleDensity);
    SPH_ASSERT(abs(int(N) - int(expectedN)) <= int(params.maxDifference));

    // generate initial particle positions
    Storage storage = generateInitial(domain, N, actDensity);
    Array<Vector>& r = storage.getValue<Vector>(QuantityId::POSITION);

    GhostParticles bc(makeAuto<ForwardingDomain>(domain), 2._f, EPS);

    UniformGridFinder finder;
    ThreadLocal<Array<NeighborRecord>> neighs(scheduler);
    finder.build(scheduler, r);

    const Float correction = params.strength / (1._f + params.small);
    // radius of search, does not have to be equal to radius of used SPH kernel
    const Float kernelRadius = 2._f;

    this->startProgress(params.numOfIters);

    Array<Vector> deltas(N);
    for (Size k = 0; k < params.numOfIters; ++k) {
        VerboseLogGuard guard("DiehlDistribution::generate - iteration " + toString(k));

        // notify caller, if requested
        if (!this->tickProgress(r)) {
            break;
        }

        // gradually decrease the strength of particle dislocation
        const Float converg = 1._f / sqrt(Float(k + 1));

        // add ghost particles
        bc.initialize(storage);

        // reconstruct finder to allow for variable topology of particles (we need to reset the internal
        // arrayview as we added ghosts)
        finder.build(scheduler, r);

        // clean up the previous displacements
        deltas.fill(Vector(0._f));

        auto lambda = [&](const Size i, Array<NeighborRecord>& neighsTl) {
            const Float rhoi = actDensity(r[i]); // average particle density
            // average interparticle distance at given point
            const Float neighborRadius = kernelRadius / root<3>(rhoi);
            finder.findAll(i, neighborRadius, neighsTl);

            for (Size j = 0; j < neighsTl.size(); ++j) {
                const Size k = neighsTl[j].index;
                const Vector diff = r[k] - r[i];
                const Float lengthSqr = getSqrLength(diff);
                // for ghost particles, just copy the density (density outside the domain is always 0)
                const Float rhok = (k >= N) ? rhoi : actDensity(r[k]);
                if (rhoi == 0._f || rhok == 0._f) {
                    // outside of the domain? do not move
                    continue;
                }
                // average kernel radius to allow for the gradient of particle density
                const Float h = kernelRadius * (0.5_f / root<3>(rhoi) + 0.5_f / root<3>(rhok));
                if (lengthSqr > h * h || lengthSqr == 0) {
                    continue;
                }
                const Float hSqrInv = 1._f / (h * h);
                const Float length = getLength(diff);
                SPH_ASSERT(length != 0._f);
                const Vector diffUnit = diff / length;
                const Float t =
                    converg * h *
                    (params.strength / (params.small + getSqrLength(diff) * hSqrInv) - correction);
                deltas[i] += diffUnit * min(t, h); // clamp the displacement to particle distance
                SPH_ASSERT(isReal(deltas[i]));
            }
            deltas[i][H] = 0._f; // do not affect smoothing lengths
        };
        parallelFor(scheduler, neighs, 0, N, 100, lambda);

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
        SPH_ASSERT(isReal(r[i]) && r[i][H] > 1.e-20_f);
    }
#endif
    // extract the buffers from the storage
    Array<Vector> positions = std::move(storage.getValue<Vector>(QuantityId::POSITION));
    return positions;
}

//-----------------------------------------------------------------------------------------------------------
// ParametrizedSpiralingDistribution implementation
//-----------------------------------------------------------------------------------------------------------

ParametrizedSpiralingDistribution::ParametrizedSpiralingDistribution(const Size seed)
    : seed(seed) {}

Array<Vector> ParametrizedSpiralingDistribution::generate(IScheduler& UNUSED(scheduler),
    const Size n,
    const IDomain& domain) const {
    const Vector center = domain.getCenter();
    const Float volume = domain.getVolume();
    const Box bbox = domain.getBoundingBox();
    const Float R = 0.5_f * maxElement(bbox.size());

    // interparticle distance based on density
    const Float h = root<3>(volume / n);
    const Size numShells = Size(R / h);
    Array<Float> shells(numShells);
    Float total = 0._f;
    for (Size i = 0; i < numShells; ++i) {
        shells[i] = sqr((i + 1) * h);
        total += shells[i];
    }
    SPH_ASSERT(isReal(total));

    Float mult = Float(n) / total;
    for (Size i = 0; i < numShells; ++i) {
        shells[i] *= mult;
    }

    this->startProgress(n);

    Array<Vector> pos;
    Float phi = 0._f;
    Size shellIdx = 0;
    UniformRng rng(seed);
    for (Float r = h; r <= R; r += h, shellIdx++) {
        Vector dir = sampleUnitSphere(rng);
        Float rot = 2._f * PI * rng();
        AffineMatrix rotator = AffineMatrix::rotateAxis(dir, rot);
        const Size m = Size(ceil(shells[shellIdx]));
        for (Size k = 1; k < m; ++k) {
            const Float hk = -1._f + 2._f * Float(k) / m;
            const Float theta = acos(hk);
            phi += 3.8_f / sqrt(m * (1._f - sqr(hk)));
            Vector v = center + rotator * sphericalToCartesian(r, theta, phi);
            if (domain.contains(v)) {
                v[H] = h; // 0.66_f * sqrt(sphereSurfaceArea(r) / m);
                SPH_ASSERT(isReal(v));
                pos.push(v);

                if (!this->tickProgress()) {
                    return {};
                }
            }
        }
    }
    return pos;
}
//-----------------------------------------------------------------------------------------------------------
// LinearDistribution implementation
//-----------------------------------------------------------------------------------------------------------

Array<Vector> LinearDistribution::generate(IScheduler& UNUSED(scheduler),
    const Size n,
    const IDomain& domain) const {
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
