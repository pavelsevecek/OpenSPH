#include "sph/initial/Galaxy.h"
#include "gravity/BarnesHut.h"
#include "gravity/BruteForceGravity.h"
#include "gravity/Moments.h"
#include "math/rng/Rng.h"
#include "quantities/IMaterial.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "system/Settings.impl.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

//-----------------------------------------------------------------------------------------------------------
// Galaxy
//-----------------------------------------------------------------------------------------------------------

// clang-format off
template<>
AutoPtr<Settings<GalaxySettingsId>> Settings<GalaxySettingsId>::instance (new Settings<GalaxySettingsId> {
    { GalaxySettingsId::DISK_PARTICLE_COUNT,    "disk.particle_count",  10000, "" },
    { GalaxySettingsId::DISK_RADIAL_CUTOFF,     "disk.radial_cutoff",   7.5_f, "" },
    { GalaxySettingsId::DISK_RADIAL_SCALE,      "disk.radial_scale",    1._f, "" },
    { GalaxySettingsId::DISK_VERTICAL_SCALE,    "disk.vertical_scale",  0.2_f, "" },
    { GalaxySettingsId::DISK_VERTICAL_CUTOFF,   "disk.vertical_cutoff", 0.6_f, "" },
    { GalaxySettingsId::DISK_TOOMRE_Q,          "disk.toomre_q",        1.2_f, "" },
    { GalaxySettingsId::DISK_MASS,              "disk.mass",            1._f, "" },
    { GalaxySettingsId::HALO_PARTICLE_COUNT,    "halo.particle_count",  10000, "" },
    { GalaxySettingsId::HALO_SCALE_LENGTH,      "halo.scale_length",    10._f, "" },
    { GalaxySettingsId::HALO_GAMMA,             "halo.gamma",           2._f, "" },
    { GalaxySettingsId::HALO_CUTOFF,            "halo.cutoff",          15._f, "" },
    { GalaxySettingsId::HALO_MASS,              "halo.mass",            5._f, "" },
    { GalaxySettingsId::BULGE_PARTICLE_COUNT,   "bulge.particle_count", 10000, "" },
    { GalaxySettingsId::BULGE_SCALE_LENGTH,     "bulge.scale_length",   0.4_f, "" },
    { GalaxySettingsId::BULGE_CUTOFF,           "bulge.cutoff",         5._f, "" },
    { GalaxySettingsId::BULGE_MASS,             "bulge.mass",           0.6_f, "" },
    { GalaxySettingsId::PARTICLE_RADIUS,        "particle_radius",      0.01_f, "" },
});
// clang-format on

// Explicit instantiation
template class Settings<GalaxySettingsId>;
template class SettingsIterator<GalaxySettingsId>;

/// Mostly uses methods described in https://github.com/nmuldavin/NBodyIntegrator

/// Surface probability distribution of a disk
INLINE Float diskSurfacePdf(const Float r, const Float h) {
    return exp(-r / h) * r;
}

/// Normalized surface density of a disk
INLINE Float diskSurfaceDensity(const Float r, const Float h, const Float m_disk) {
    return m_disk / (2._f * PI * sqr(h)) * exp(-r / h);
}

/// Vertical mass distribution of a disk
INLINE Float diskVerticalPdf(const Float z, const Float z0) {
    return 1._f / sqr(cosh(z / z0));
}

/// Probability distribution function of a halo
INLINE Float haloPdf(const Float r, const Float r0, const Float g0) {
    return exp(-sqr(r / r0)) / (sqr(r) + sqr(g0)) * sqr(r);
}

INLINE Float maxHaloPdf(const Float r0, const Float g0) {
    const Float x2 = 0.5_f * (sqrt(sqr(g0) * (sqr(g0) + 4._f * sqr(r0))) - sqr(g0));
    SPH_ASSERT(x2 > 0._f);
    return haloPdf(sqrt(x2), r0, g0);
}

/// Probability distribution function for velocities in halo and bulge.
INLINE Float velocityPdf(const Float v, const Float sigma_r) {
    return sqr(v) * exp(-0.5_f * sqr(v) / sigma_r);
}

/// Probability distribution function of a bulge
INLINE Float bulgePdf(const Float r, const Float a) {
    return r / (sqr(a) * pow<3>(1._f + r / a));
}

static Float getEpicyclicFrequency(IGravity& gravity, const Vector& r, const Vector& dv1, const Float dr) {
    const Float radius = sqrt(sqr(r[X]) + sqr(r[Y])) + EPS;
    const Vector dv2 = gravity.eval(r * (1._f + dr));

    const Float a1_rad = (dv1[X] * r[X] + dv1[Y] * r[Y]) / radius;
    const Float a2_rad = (dv2[X] * r[X] + dv2[Y] * r[Y]) / radius;

    const Float k2 = (3._f / radius) * a1_rad + (a2_rad - a1_rad) / dr;
    return sqrt(abs(k2));
}

Storage Galaxy::generateDisk(UniformRng& rng, const GalaxySettings& settings) {
    MEASURE_SCOPE("Galaxy::generateDisk");

    Array<Vector> positions;
    const Size n_disk = settings.get<int>(GalaxySettingsId::DISK_PARTICLE_COUNT);
    const Float r_cutoff = settings.get<Float>(GalaxySettingsId::DISK_RADIAL_CUTOFF);
    const Float r0 = settings.get<Float>(GalaxySettingsId::DISK_RADIAL_SCALE);
    const Float z_cutoff = settings.get<Float>(GalaxySettingsId::DISK_VERTICAL_CUTOFF);
    const Float z0 = settings.get<Float>(GalaxySettingsId::DISK_VERTICAL_SCALE);
    const Float h = settings.get<Float>(GalaxySettingsId::PARTICLE_RADIUS);

    const Interval radialRange(0._f, r_cutoff);
    const Interval verticalRange(-z_cutoff, z_cutoff);

    // radial PDF is maximal at r = r0
    const Float maxSurfacePdf = diskSurfacePdf(r0, r0);

    // vertical pdf is maximal at z = 0
    const Float maxVerticalPdf = diskVerticalPdf(0, z0);

    for (Size i = 0; i < n_disk; ++i) {
        const Float r = sampleDistribution(
            rng, radialRange, maxSurfacePdf, [r0](const Float x) { return diskSurfacePdf(x, r0); });

        const Float phi = rng() * 2._f * PI;

        const Float z = sampleDistribution(
            rng, verticalRange, maxVerticalPdf, [z0](const Float x) { return diskVerticalPdf(x, z0); });

        Vector pos = cylindricalToCartesian(r, phi, z);
        pos[H] = h;
        positions.push(pos);
    }


    const Float m_disk = settings.get<Float>(GalaxySettingsId::DISK_MASS);
    const Float m = m_disk / n_disk;

    Storage storage(makeShared<NullMaterial>(BodySettings::getDefaults()));
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(positions));
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m);
    storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Size(PartEnum::DISK));
    return storage;
}

Storage Galaxy::generateHalo(UniformRng& rng, const GalaxySettings& settings) {
    MEASURE_SCOPE("Galaxy::generateHalo");

    const Size n_halo = settings.get<int>(GalaxySettingsId::HALO_PARTICLE_COUNT);
    const Float cutoff = settings.get<Float>(GalaxySettingsId::HALO_CUTOFF);
    const Float r0 = settings.get<Float>(GalaxySettingsId::HALO_SCALE_LENGTH);
    const Float g0 = settings.get<Float>(GalaxySettingsId::HALO_GAMMA);
    const Float h = settings.get<Float>(GalaxySettingsId::PARTICLE_RADIUS);
    const Interval range(0._f, cutoff);

    const Float maxPdf = maxHaloPdf(r0, g0);

    Array<Vector> positions;
    for (Size i = 0; i < n_halo; ++i) {
        const Float r = sampleDistribution(rng, range, maxPdf, [r0, g0](const Float x) { //
            return haloPdf(x, r0, g0);
        });

        Vector pos = sampleUnitSphere(rng) * r;
        pos[H] = h;
        positions.push(pos);
    }

    const Float m_halo = settings.get<Float>(GalaxySettingsId::HALO_MASS);
    const Float m = m_halo / n_halo;

    Storage storage(makeShared<NullMaterial>(BodySettings::getDefaults()));
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(positions));
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m);
    storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Size(PartEnum::HALO));
    return storage;
}

Storage Galaxy::generateBulge(UniformRng& rng, const GalaxySettings& settings) {
    MEASURE_SCOPE("Galaxy::generateBulge");

    const Size n_bulge = settings.get<int>(GalaxySettingsId::HALO_PARTICLE_COUNT);
    const Float cutoff = settings.get<Float>(GalaxySettingsId::BULGE_CUTOFF);
    const Float a = settings.get<Float>(GalaxySettingsId::BULGE_SCALE_LENGTH);
    const Float h = settings.get<Float>(GalaxySettingsId::PARTICLE_RADIUS);
    const Interval range(0._f, cutoff);

    // PDF is maximal at x=a/2
    const Float maxPdf = bulgePdf(0.5_f * a, a);

    Array<Vector> positions;
    for (Size i = 0; i < n_bulge; ++i) {
        const Float r = sampleDistribution(rng, range, maxPdf, [a](const Float x) { return bulgePdf(x, a); });

        Vector pos = sampleUnitSphere(rng) * r;
        pos[H] = h;
        positions.push(pos);
    }

    const Float m_bulge = settings.get<Float>(GalaxySettingsId::BULGE_MASS);
    const Float m = m_bulge / n_bulge;

    Storage storage(makeShared<NullMaterial>(BodySettings::getDefaults()));
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(positions));
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, m);
    storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Size(PartEnum::BULGE));
    return storage;
}

static Array<Pair<Float>> computeCumulativeMass(const GalaxySettings& settings, const Storage& storage) {
    MEASURE_SCOPE("computeCumulativeMass");

    constexpr Size MASS_BINS = 1000;

    const Float haloCutoff = settings.get<Float>(GalaxySettingsId::HALO_CUTOFF);
    const Float dr = haloCutoff / MASS_BINS;

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);

    Array<Pair<Float>> cumulativeDist;
    Array<Float> differentialDist(MASS_BINS);
    differentialDist.fill(0._f);
    for (Size i = 0; i < r.size(); ++i) {
        const Float radius = getLength(r[i]);
        const Size binIdx = Size(radius * MASS_BINS / haloCutoff);
        SPH_ASSERT(binIdx < MASS_BINS);

        differentialDist[min(binIdx, MASS_BINS - 1)] += m[i];
    }

    Float massSum = 0._f;
    for (Size binIdx = 0; binIdx < MASS_BINS; ++binIdx) {
        const Float binRadius = (binIdx + 1) * dr;
        massSum += differentialDist[binIdx];
        cumulativeDist.push(Pair<Float>{ binRadius, massSum });
    }

    return cumulativeDist;
}

static IndexSequence getPartSequence(const Storage& storage, const Galaxy::PartEnum id) {
    ArrayView<const Size> flag = storage.getValue<Size>(QuantityId::FLAG);
    Iterator<const Size> iterFrom, iterTo;
    std::tie(iterFrom, iterTo) = std::equal_range(flag.begin(), flag.end(), Size(id));
    return IndexSequence(
        Size(std::distance(flag.begin(), iterFrom)), Size(std::distance(flag.begin(), iterTo)));
}

static void computeDiskVelocities(IScheduler& scheduler,
    UniformRng& rng,
    const GalaxySettings& settings,
    Storage& storage) {
    MEASURE_SCOPE("computeDiskVelocities");

    const Float r0 = settings.get<Float>(GalaxySettingsId::DISK_RADIAL_SCALE);
    const Float z0 = settings.get<Float>(GalaxySettingsId::DISK_VERTICAL_SCALE);
    const Float r_ref = 2.5_f * r0;
    const Float r_cutoff = settings.get<Float>(GalaxySettingsId::DISK_RADIAL_CUTOFF);
    const Float m_disk = settings.get<Float>(GalaxySettingsId::DISK_MASS);
    const Float Q = settings.get<Float>(GalaxySettingsId::DISK_TOOMRE_Q);
    // const Float double as = 0.25 * h;
    const Float dr = 1.e-3_f * r_cutoff;
    const Float as = 0.25_f * r0;

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

    const IndexSequence sequence = getPartSequence(storage, Galaxy::PartEnum::DISK);

    BarnesHut gravity(0.8_f, MultipoleOrder::OCTUPOLE, SolidSphereKernel{}, 25, 50, 1._f);
    // BruteForceGravity gravity(SolidSphereKernel{}, 1._f);
    gravity.build(scheduler, storage);
    Statistics stats;
    std::fill(dv.begin(), dv.end(), Vector(0._f));
    gravity.evalAll(scheduler, dv, stats);

    Float sigma = 0._f;
    Size count = 0;
    Float annulus = dr;
    while (count == 0._f) {
        for (Size i : sequence) {
            const Float radius = sqrt(sqr(r[i][X]) + sqr(r[i][Y]));
            if (abs(radius - r_ref) < annulus) {
                const Float kappa = getEpicyclicFrequency(gravity, r[i], dv[i], 0.05_f * annulus);
                sigma += 3.36_f * diskSurfaceDensity(radius, r0, m_disk) / kappa;
                count++;
            }
        }
        // if no particle lies in the annulus, try again with larger value
        annulus *= 2._f;
    }
    SPH_ASSERT(count > 0);

    sigma = sigma * Q / count;

    const Float A = sqr(sigma) / diskSurfaceDensity(r_ref, r0, m_disk);
    SPH_ASSERT(A >= 0._f, A);

    parallelFor(scheduler, sequence, [&](const Size i) {
        const Float radius = sqrt(sqr(r[i][X]) + sqr(r[i][Y]));
        const Float vz2 = PI * z0 * diskSurfaceDensity(sqrt(sqr(radius) + 2._f * sqr(as)), r0, m_disk);
        const Float vz = sampleNormalDistribution(rng, 0._f, vz2);
        SPH_ASSERT(vz2 > 0._f);

        const Float vr2 = A * vz2 / (PI * z0);
        const Float vr = sampleNormalDistribution(rng, 0._f, vr2);
        SPH_ASSERT(vr2 > 0._f);

        const Vector a = dv[i];
        const Float ar = (a[X] * r[i][X] + a[Y] * r[i][Y]) / radius;
        SPH_ASSERT(isReal(ar));

        const Float omega = sqrt(abs(ar) / radius);
        SPH_ASSERT(isReal(omega));

        const Float kappa = getEpicyclicFrequency(gravity, r[i], dv[i], dr);
        SPH_ASSERT(isReal(kappa));

        //  circular velocity
        const Float v_c = omega * radius;
        Float va = sqrt(abs(sqr(v_c) + vr2 * (1._f - sqr(kappa) / (4._f * sqr(omega)) - 2._f * radius / r0)));
        SPH_ASSERT(isReal(va));

        const Float sigma2 = vr2 * sqr(kappa) / (4._f * sqr(omega));
        va += sampleNormalDistribution(rng, 0._f, sigma2);

        //  transform to cartesian coordinates
        const Float c = r[i][X] / radius;
        const Float s = r[i][Y] / radius;
        v[i][X] = vr * c - va * s;
        v[i][Y] = vr * s + va * c;
        v[i][Z] = vz;
    });
}

template <typename TFunc>
static void computeSphericalVelocities(UniformRng& rng,
    ArrayView<const Pair<Float>> massDist,
    const Galaxy::PartEnum partId,
    Storage& storage,
    const TFunc& func) {
    const Float dr = massDist[1][0] - massDist[0][0];

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);

    const IndexSequence sequence = getPartSequence(storage, partId);
    for (Size i : sequence) {
        const Float radius = getLength(r[i]);
        const Size firstBin = Size(radius / dr);

        const Float v_esc = sqrt(2._f * massDist[firstBin][1] / radius);

        Float vr2 = 0._f;
        for (Size binIdx = firstBin; binIdx < massDist.size(); ++binIdx) {
            vr2 += func(massDist[binIdx][0]) * dr * massDist[binIdx][1];
        }
        vr2 /= func(radius) / sqr(radius);

        const Interval range(0._f, 0.95_f * v_esc);
        const Float maxPdf = velocityPdf(sqrt(2._f * vr2), vr2);

        const Float u = sampleDistribution(rng, range, maxPdf, [vr2](const Float x) { //
            return velocityPdf(x, vr2);
        });

        v[i] = sampleUnitSphere(rng) * u;
    }
}

static void computeHaloVelocities(UniformRng& rng,
    const GalaxySettings& settings,
    ArrayView<const Pair<Float>> massDist,
    Storage& storage) {
    MEASURE_SCOPE("computeHaloVelocities");

    const Float r0 = settings.get<Float>(GalaxySettingsId::HALO_SCALE_LENGTH);
    const Float g0 = settings.get<Float>(GalaxySettingsId::HALO_GAMMA);

    computeSphericalVelocities(rng, massDist, Galaxy::PartEnum::HALO, storage, [r0, g0](const Float x) { //
        return haloPdf(x, r0, g0);
    });
}

static void computeBulgeVelocities(UniformRng& rng,
    const GalaxySettings& settings,
    ArrayView<const Pair<Float>> massDist,
    Storage& storage) {
    MEASURE_SCOPE("computeBulgeVelocities");

    const Float a = settings.get<Float>(GalaxySettingsId::BULGE_SCALE_LENGTH);

    computeSphericalVelocities(rng, massDist, Galaxy::PartEnum::BULGE, storage, [a](const Float x) { //
        return bulgePdf(x, a);
    });
}

class StorageBuilder {
public:
    Storage storage;
    const Galaxy::IProgressCallbacks& callbacks;
    Size partId = 0;

    const Size numParts = 8;

public:
    StorageBuilder(const Galaxy::IProgressCallbacks& callbacks)
        : callbacks(callbacks) {}

    Storage& operator*() {
        callbacks.onPart(storage, partId, numParts);
        ++partId;

        return storage;
    }

    Storage* operator->() {
        return &operator*();
    }

    Storage release() && {
        return std::move(storage);
    }
};

Storage Galaxy::generateIc(const RunSettings& globals,
    const GalaxySettings& settings,
    const IProgressCallbacks& callbacks) {
    const int seed = globals.get<int>(RunSettingsId::RUN_RNG_SEED);
    UniformRng rng(seed);
    SharedPtr<IScheduler> scheduler = Factory::getScheduler(globals);

    StorageBuilder builder(callbacks);
    builder->merge(generateDisk(rng, settings));
    builder->merge(generateHalo(rng, settings));
    builder->merge(generateBulge(rng, settings));

    Array<Pair<Float>> massDist = computeCumulativeMass(settings, *builder);
    computeDiskVelocities(*scheduler, rng, settings, *builder);
    computeHaloVelocities(rng, settings, massDist, *builder);
    computeBulgeVelocities(rng, settings, massDist, *builder);

    Storage storage = std::move(builder).release();
    ArrayView<const Size> flag = storage.getValue<Size>(QuantityId::FLAG);
    SPH_ASSERT(std::is_sorted(flag.begin(), flag.end()));
    return storage;
}

NAMESPACE_SPH_END
