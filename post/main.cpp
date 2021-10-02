/// Just a cluster of auxiliary tools, used for analysis of the result.
///
/// Currently fixed for my machine, sorry :(

#include "io/FileManager.h"
#include "io/FileSystem.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "objects/Exceptions.h"
#include "objects/containers/ArrayRef.h"
#include "physics/Functions.h"
#include "post/Analysis.h"
#include "post/StatisticTests.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "quantities/Utility.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "system/Process.h"
#include "system/Statistics.h"
#include "thread/Pool.h"
#include <fstream>
#include <iostream>
#include <mutex>

using namespace Sph;

static Expected<Storage> parsePkdgravOutput(const Path& path) {
    Storage storage;
    Statistics stats;
    PkdgravInput io;
    Outcome result = io.load(path, storage, stats);
    if (!result) {
        return makeUnexpected<Storage>(result.error());
    } else {
        return storage;
    }
}

int pkdgravToSfd(const Path& filePath, const Path& sfdPath) {
    std::cout << "Processing pkdgrav file ... " << std::endl;
    Expected<Storage> storage = parsePkdgravOutput(filePath);
    if (!storage) {
        std::wcout << "Invalid file: " << storage.error() << std::endl;
        return 0;
    }
    Post::HistogramParams params;
    Array<Post::HistPoint> sfd = Post::getCumulativeHistogram(
        storage.value(), Post::HistogramId::EQUIVALENT_MASS_RADII, Post::HistogramSource::PARTICLES, params);
    FileLogger logRadiiSfd(sfdPath, EMPTY_FLAGS);
    for (Post::HistPoint& p : sfd) {
        logRadiiSfd.write(p.value, "  ", p.count);
    }
    return 0;
}

int pkdgravToOmega(const Path& filePath, const Path& omegaPath) {
    std::cout << "Processing pkdgrav file ... " << std::endl;
    Expected<Storage> storage = parsePkdgravOutput(filePath);
    if (!storage) {
        std::wcout << "Invalid file: " << storage.error() << std::endl;
        return 0;
    }
    /*Post::HistogramParams params;
    params.source = Post::HistogramParams::Source::PARTICLES;
    params.id = Post::HistogramId::ANGULAR_VELOCITIES;
    params.binCnt = 50;
    params.validator = [](const Float value) { return value > 0._f; };*/

    // Array<Post::SfdPoint> sfd = Post::getDifferentialSfd(storage.value(), params);
    FileLogger logOmegaSfd(omegaPath, EMPTY_FLAGS);
    /*for (Post::SfdPoint& p : sfd) {
        logOmegaSfd.write(p.value, "  ", p.count);
    }*/
    ArrayView<Vector> omega = storage->getValue<Vector>(QuantityId::ANGULAR_FREQUENCY);
    std::sort(
        omega.begin(), omega.end(), [](Vector& v1, Vector& v2) { return getLength(v1) > getLength(v2); });
    for (Vector v : omega) {
        if (getLength(v) != 0._f) {
            logOmegaSfd.write(getLength(v));
        }
    }
    return 0;
}

int pkdgravToMoons(const Path& filePath, const Float limit) {
    std::cout << "Processing pkdgrav file ... " << std::endl;
    Expected<Storage> storage = parsePkdgravOutput(filePath);
    if (!storage) {
        std::cout << "Invalid file: " << storage.error() << std::endl;
        return 0;
    }
    /// \todo use correct radius here, we assume that very close ecounters will eventually collide
    Array<Post::MoonEnum> moons = Post::findMoons(storage.value(), 1.2_f, limit);
    Size moonCnt = std::count(moons.begin(), moons.end(), Post::MoonEnum::MOON);
    std::cout << "Moon count = " << moonCnt << std::endl;
    return 0;
}

int ssfToSfd(const Post::HistogramSource source, const Path& filePath, const Path& sfdPath) {
    std::cout << "Processing SPH file ... " << std::endl;
    AutoPtr<IInput> input = Factory::getInput(filePath);
    Storage storage;
    Statistics stats;
    Outcome outcome = input->load(filePath, storage, stats);
    if (!outcome) {
        std::cout << "Cannot load particle data, " << outcome.error() << std::endl;
        return 0;
    }

    Post::HistogramParams params;
    Array<Post::HistPoint> sfd =
        Post::getCumulativeHistogram(storage, Post::HistogramId::EQUIVALENT_MASS_RADII, source, params);
    FileLogger logSfd(sfdPath, EMPTY_FLAGS);
    for (Post::HistPoint& p : sfd) {
        logSfd.write(p.value, "  ", p.count);
    }
    return 0;
}

int ssfToOmega(const Path& filePath,
    const Path& omegaPath,
    const Path& omegaDPath,
    const Path& omegaDirPath) {
    std::cout << "Processing SPH file ... " << std::endl;
    BinaryInput input;
    Storage storage;
    Statistics stats;
    Outcome outcome = input.load(filePath, storage, stats);
    if (!outcome) {
        std::cout << "Cannot load particle data, " << outcome.error() << std::endl;
        return 0;
    }

    Post::HistogramParams params;
    params.range = Interval(0._f, 13._f);
    params.binCnt = 12;

    ArrayView<const Vector> w = storage.getValue<Vector>(QuantityId::ANGULAR_FREQUENCY);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    // const Float massCutoff = 1._f / 300000._f;
    const Float m_total = std::accumulate(m.begin(), m.end(), 0._f);
    params.validator = [w](const Size i) { //
        return getSqrLength(w[i]) > 0._f;  //= m_total * massCutoff;
    };

    params.centerBins = false;

    Array<Post::HistPoint> sfd = Post::getDifferentialHistogram(
        storage, Post::HistogramId::ROTATIONAL_FREQUENCY, Post::HistogramSource::PARTICLES, params);

    FileLogger logOmega(omegaPath, EMPTY_FLAGS);
    for (Post::HistPoint& p : sfd) {
        logOmega.write(p.value, "  ", p.count); // / sum);
    }

    params.range = Interval();
    Array<Post::HistPoint> dirs = Post::getDifferentialHistogram(
        storage, Post::HistogramId::ROTATIONAL_AXIS, Post::HistogramSource::PARTICLES, params);

    FileLogger logOmegaDir(omegaDirPath, EMPTY_FLAGS);
    for (Post::HistPoint& p : dirs) {
        logOmegaDir.write(p.value, "  ", p.count);
    }

    // print omega-D relation
    Array<Float> h(storage.getParticleCnt());
    /// ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < m.size(); ++i) {
        h[i] = r[i][H]; // root<3>(3.f * m[i] / (rho[i] * 4.f * PI));
    }


    FileLogger logOmegaD(omegaDPath, EMPTY_FLAGS);
    for (Size i = 0; i < m.size(); ++i) {
        if (m[i] > 3._f * params.massCutoff * m_total) {
            // in m vs. rev/day
            logOmegaD.write(2._f * h[i], "  ", getLength(w[i]) * 3600._f * 24._f / (2._f * PI));
        }
    }

    return 0;
}


int ssfToVelocity(const Path& filePath, const Path& outPath) {
    std::cout << "Processing SPH file ... " << std::endl;
    AutoPtr<IInput> input = Factory::getInput(filePath);
    Storage storage;
    Statistics stats;
    Outcome outcome = input->load(filePath, storage, stats);
    if (!outcome) {
        std::cout << "Cannot load particle data, " << outcome.error() << std::endl;
        return -1;
    }

    // convert to system with center at LR
    Array<Size> idxs = Post::findLargestComponent(storage, 2._f, EMPTY_FLAGS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    Vector r0(0._f);
    Vector v0(0._f);
    Float m0 = 0._f;
    for (Size i : idxs) {
        m0 += m[i];
        r0 += m[i] * r[i];
        v0 += m[i] * v[i];
    }
    r0 /= m0;
    v0 /= m0;

    for (Size i = 0; i < r.size(); ++i) {
        r[i] -= r0;
        v[i] -= v0;
    }

    Post::HistogramParams params;
    params.binCnt = 2000;
    Array<Post::HistPoint> hist = Post::getDifferentialHistogram(
        storage, Post::HistogramId::VELOCITIES, Post::HistogramSource::COMPONENTS, params);

    FileLogger logSfd(outPath, EMPTY_FLAGS);
    for (Post::HistPoint& p : hist) {
        logSfd.write(p.value, "  ", p.count);
    }

    return 0;
}

void ssfToVelDir(const Path& filePath, const Path& outPath) {
    std::cout << "Processing SPH file ... " << std::endl;
    BinaryInput input;
    Storage storage;
    Statistics stats;
    Outcome outcome = input.load(filePath, storage, stats);
    if (!outcome) {
        std::cout << "Cannot load particle data, " << outcome.error() << std::endl;
        return;
    }

    // convert to system with center at LR
    Array<Size> idxs = Post::findLargestComponent(storage, 2._f, EMPTY_FLAGS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    Vector r0(0._f);
    Vector v0(0._f);
    Float m0 = 0._f;
    for (Size i : idxs) {
        m0 += m[i];
        r0 += m[i] * r[i];
        v0 += m[i] * v[i];
    }
    r0 /= m0;
    v0 /= m0;

    for (Size i = 0; i < r.size(); ++i) {
        r[i] -= r0;
        v[i] -= v0;
    }

    // compute velocity directions (in x-y plane)
    Array<Float> dirs;
    for (Size i = 0; i < v.size(); ++i) {
        Float dir = Sph::atan2(v[i][Y], v[i][X]);
        if (dir < 0._f) {
            dir += 2._f * PI;
        }
        dirs.push(dir * RAD_TO_DEG);
    }

    Post::HistogramParams params;
    params.range = Interval(0._f, 360._f);
    params.binCnt = 72; // 5 deg bins
    Array<Post::HistPoint> hist = Post::getDifferentialHistogram(dirs, params);
    FileLogger logSfd(outPath, EMPTY_FLAGS);
    for (Post::HistPoint& p : hist) {
        logSfd.write(p.value, "  ", p.count);
    }
}

struct HarrisAsteroid {
    Optional<Size> number;
    String name;
    Optional<Float> radius;
    Optional<Float> period;
};

static Array<HarrisAsteroid> loadHarris(std::wifstream& ifs) {
    Array<HarrisAsteroid> harris;
    while (ifs) {
        String dummy;
        String number;
        ifs >> number >> dummy;

        String name;
        ifs >> name;
        for (int i = 0; i < 6; ++i) {
            ifs >> dummy;
        }

        String radius;
        ifs >> radius;
        for (int i = 0; i < 5; ++i) {
            ifs >> dummy;
        }

        String period;
        ifs >> period;
        for (int i = 0; i < 10; ++i) {
            ifs >> dummy;
        }

        harris.push(HarrisAsteroid{
            fromString<Size>(number),
            name,
            fromString<Float>(radius),
            fromString<Float>(period),
        });
    }
    return harris;
}

struct FamilyAsteroid {
    Optional<Size> number;
    Optional<String> name;
};

static Array<FamilyAsteroid> loadFamilies(std::wifstream& ifs) {
    SPH_ASSERT(ifs);
    Array<FamilyAsteroid> asteroids;
    std::wstring line;
    bool firstLine = true;
    int format = 1;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') {
            // comment line
            if (firstLine) {
                // this is the other format of the file, with asteroid names, etc.
                format = 2;
            }
            firstLine = false;
            continue;
        }
        firstLine = false;
        std::wstringstream ss(line);
        // both formats start with asteroid number
        String number;
        ss >> number;
        if (format == 2) {
            String name;
            ss >> name;
            asteroids.push(FamilyAsteroid{ fromString<Size>(number), name });
        } else {
            asteroids.push(FamilyAsteroid{ fromString<Size>(number), NOTHING });
        }
        // check that we have at least one information
        SPH_ASSERT(asteroids.back().name || asteroids.back().number);
    }
    return asteroids;
}

static Optional<HarrisAsteroid> findInHarris(const FamilyAsteroid& ast1,
    ArrayView<const HarrisAsteroid> catalog) {
    auto iter = std::find_if(catalog.begin(), catalog.end(), [&ast1](const HarrisAsteroid& ast2) { //
        // search primarily by number
        if (ast1.number && ast2.number && ast1.number.value() == ast2.number.value()) {
            return true;
        }
        // if we don't have the number, search by name
        if (ast1.name && ast1.name.value() == ast2.name) {
            return true;
        }
        // either don't match or we don't have the information
        return false;
    });
    if (iter == catalog.end()) {
        return NOTHING;
    } else {
        if (iter->period && iter->radius) {
            return *iter;
        } else {
            return NOTHING;
        }
    }
}

static Optional<Post::KsResult> printDvsOmega(const Path& familyData,
    const Path& outputPath,
    ArrayView<const HarrisAsteroid> catalog,
    Array<PlotPoint>& outPoints) {

    std::wifstream ifs(familyData.native());
    SPH_ASSERT(ifs);
    Array<FamilyAsteroid> family = loadFamilies(ifs);
    Array<HarrisAsteroid> found;
    Interval rangePeriod;
    Interval rangeR;
    for (FamilyAsteroid& famAst : family) {
        if (auto harAst = findInHarris(famAst, catalog)) {
            found.push(harAst.value());
            rangePeriod.extend(harAst->period.value());
            rangeR.extend(harAst->radius.value());
        }
    }
    if (found.size() < 10) {
        // too few data to do any statistics
        return NOTHING;
    }

    FileSystem::createDirectory(outputPath.parentPath());
    std::wofstream ofs(outputPath.native());
    SPH_ASSERT(ofs);

    auto compare = [](HarrisAsteroid& ast1, HarrisAsteroid& ast2) {
        return ast1.radius.value() < ast2.radius.value();
    };
    Iterator<HarrisAsteroid> largestRemnant = std::max_element(found.begin(), found.end(), compare);

    auto periodToOmega = [](const Float P) { return 1._f / (P / 24._f); };

    Array<PlotPoint> points;
    for (HarrisAsteroid& ast : found) {
        const Float omega = periodToOmega(ast.period.value());
        if (&ast != &*largestRemnant) {
            String printedName = ast.name;
            if (ast.number) {
                printedName = "(" + toString(ast.number.value()) + ") " + printedName;
            }
            ofs << ast.radius.value() << "  " << omega << "   " << printedName << std::endl;
        }
        points.push(PlotPoint(ast.radius.value(), omega));
    }
    Interval rangeOmega;
    rangeOmega.extend(periodToOmega(rangePeriod.lower()));
    rangeOmega.extend(periodToOmega(rangePeriod.upper()));

    Post::KsFunction uniformCdf = Post::getUniformKsFunction(rangeR, rangeOmega);
    Post::KsResult result = Post::kolmogorovSmirnovTest(points, uniformCdf);


    if (points.size() > 36) {
        const Path histPath = Path("histogram") / outputPath.fileName();
        FileSystem::createDirectory(histPath.parentPath());
        std::wofstream histofs(histPath.native());
        Array<Float> values;
        for (PlotPoint& p : points) {
            values.push(p.y);
        }
        Post::HistogramParams params;
        params.range = Interval(0._f, 13._f);
        Array<Post::HistPoint> histogram = Post::getDifferentialHistogram(values, params);
        for (Post::HistPoint p : histogram) {
            histofs << p.value << "  " << p.count << std::endl;
        }
    }

    outPoints = std::move(points);
    return result;
}

static String elliptize(const String& s, const Size maxSize) {
    SPH_ASSERT(maxSize >= 5);
    if (s.size() < maxSize) {
        return s;
    }
    const Size cutSize = (maxSize - 3) / 2;
    return s.substr(0, cutSize) + "..." + s.substr(s.size() - cutSize);
}

void processHarrisFile() {
    Path harrisPath("/home/pavel/projects/astro/asteroids/grant3/harris.out");
    std::wifstream ifs(harrisPath.native());
    Array<HarrisAsteroid> harris = loadHarris(ifs);
    ifs.close();

    Size below3 = 0, below7 = 0, below12 = 0, total = 0;
    Float avgPeriod = 0._f;
    Size count = 0;
    for (auto& h : harris) {
        if (!h.period) {
            continue;
        }

        if (h.period.value() < 3) {
            below3++;
        }
        if (h.period.value() < 7) {
            below7++;
        }
        if (h.period.value() < 12) {
            below12++;
        }

        if (h.radius.valueOr(0._f) > 100._f) {
            avgPeriod += h.period.value();
            count++;
        }
        total++;
    }
    std::cout << "Below 3h: " << (100.f * below3) / total << "%" << std::endl;
    std::cout << "Below 7h: " << (100.f * below7) / total << "%" << std::endl;
    std::cout << "Below 12h: " << (100.f * below12) / total << "%" << std::endl;

    if (count > 0) {
        std::cout << "Average period for R>100km: " << avgPeriod / count << std::endl;
    } else {
        SPH_ASSERT(false);
    }

    Array<PlotPoint> points;
    printDvsOmega(
        Path("/home/pavel/projects/astro/asteroids/families.txt"), Path("LR_D_omega.txt"), harris, points);

    const Path parentPath("/home/pavel/projects/astro/asteroids/families");
    Array<Path> paths = FileSystem::getFilesInDirectory(parentPath);
    std::mutex printMutex;
    std::mutex plotMutex;
    UniquePathManager uniquePaths;
    std::ofstream kss("KS.txt");
    kss << "# name                             D_ks           probability" << std::endl;

    std::ofstream alls("D_omega_all.txt");

    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    parallelFor(pool, 0, paths.size(), [&](const Size index) {
        {
            std::unique_lock<std::mutex> lock(printMutex);
            std::wcout << paths[index].string() << std::endl;
        }
        const Path name = paths[index].fileName();
        const Path targetPath = Path("D_omega") / Path(name.string() + ".txt");
        const Path ksPath = Path("KS") / Path(name.string() + ".txt");
        Array<PlotPoint> points;
        Optional<Post::KsResult> ks = printDvsOmega(parentPath / paths[index], targetPath, harris, points);

        std::unique_lock<std::mutex> lock(plotMutex);
        if (!FileSystem::pathExists(targetPath) || FileSystem::fileSize(targetPath) == 0) {
            return;
        }
        if (ks) {
            kss << std::left << std::setw(35) << elliptize(Path(name).removeExtension().string(), 30)
                << std::setw(15) << ks->D << std::setw(15) << ks->prob << std::endl;
        }
        for (PlotPoint p : points) {
            alls << p.x << "  " << p.y << "  " << index << std::endl;
        }

        FileSystem::copyFile(targetPath, Path("family.txt"));
        // make plot
        Process gnuplot(Path("/bin/gnuplot"), { "doplot.plt" });
        gnuplot.wait();
        SPH_ASSERT(FileSystem::pathExists(Path("plot.png")));
        FileSystem::copyFile(Path("plot.png"), uniquePaths.getPath(Path(targetPath).replaceExtension("png")));

        if (points.size() > 25) {
            const Path histPath = Path("histogram") / targetPath.fileName();
            FileSystem::copyFile(histPath, Path("hist.txt"));
            Process gnuplot2(Path("/bin/gnuplot"), { "dohistogram.plt" });
            gnuplot2.wait();
            SPH_ASSERT(FileSystem::pathExists(Path("hist.png")));
            FileSystem::copyFile(
                Path("hist.png"), uniquePaths.getPath(Path(histPath).replaceExtension("png")));
        }
    });

    FileSystem::copyFile(Path("D_omega_all.txt"), Path("family.txt"));
    Process gnuplot(Path("/bin/gnuplot"), { "doplot_all.plt" });
    gnuplot.wait();
}

static Float maxwellBoltzmann(const Float x, const Float a) {
    return sqrt(2._f / PI) * sqr(x) * exp(-sqr(x) / (2._f * sqr(a))) / pow<3>(a);
}

static Float sampleMaxwellBoltzmann(UniformRng& rng, const Float a) {
    while (true) {
        const Float x = rng() * a * 10._f;
        const Float y = rng() / a;

        if (maxwellBoltzmann(x, a) > y) {
            return x;
        }
    }
}

void makeSwift(const Path& filePath) {
    // for Hygiea
    /*const Float a = 3.14178_f;
    const Float e = 0.135631_f;
    const Float I = asin(0.0889622);
    const Float W = 64.621768_f * DEG_TO_RAD;
    const Float w = 128.543611_f * DEG_TO_RAD;
    const Float u = 0._f;

    const Float X = (cos(W) * cos(w) - sin(W) * cos(I) * sin(w)) * a * (cos(u) - e) -
                    (cos(W) * sin(w) + sin(W) * cos(I) * cos(w)) * a * sqrt(1 - sqr(e)) * sin(u);
    const Float Y = (sin(W) * cos(w) + cos(W) * cos(I) * sin(w)) * a * (cos(u) - e) +
                    (-sin(W) * sin(w) + cos(W) * cos(I) * cos(w)) * a * sqrt(1 - sqr(e)) * sin(u);
    const Float Z = sin(I) * sin(w) * a * (cos(u) - e) + sin(I) * cos(w) * a * sqrt(1 - sqr(e)) * sin(u);

    const Vector r = Vector(X, Y, Z);

    const Float npart = 1500;

    BinaryInput input;
    Storage storage;
    Statistics stats;
    if (!input.load(filePath, storage, stats)) {
        std::cout << "Cannot parse ssf file" << std::endl;
    }

    ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);

    FileLogger logger(Path("tp.in"));
    UniformRng rng;
    logger.write(npart);
    for (Size i = 0; i < npart; ++i) {
        logger.write(r / Constants::au);
        const Size idx = clamp<Size>(Size(rng() * v.size()), 0, storage.getParticleCnt() - 1);
        logger.write(v[idx] / Constants::au * Constants::year);
        logger.write("0");
        logger.write("0.0");
    }*/
    std::wifstream ifs(filePath.native());
    std::wstring line;

    Array<Float> rs;
    while (std::getline(ifs, line)) {
        const Float d = std::stof(line);
        rs.push(d / 2 * 1000);
    }

    std::ofstream yarko("yarko.in");
    yarko << rs.size() << std::endl;
    for (Float r : rs) {
        yarko << r << " 2860.0 1500.0 0.0010 680.0 0.10 0.90" << std::endl;
    }

    std::ofstream spin("spin.in");
    spin << rs.size() << "\n-1\n1\n";
    UniformRng rng;
    for (Size i = 0; i < rs.size(); ++i) {
        const Float phi = rng() * 2._f * PI;
        const Float cosTheta = rng() * 2._f - 1._f;
        const Float theta = Sph::acos(cosTheta);
        spin << sphericalToCartesian(1._f, theta, phi) << "  " << sampleMaxwellBoltzmann(rng, 0.0001_f)
             << std::endl;
    }

    std::ofstream yorp("yorp.in");
    for (Size i = 1; i <= rs.size(); ++i) {
        yorp << i << "  " << clamp(int(rng() * 200), 0, 199) << std::endl;
    }
}

void origComponents(const Path& lastDumpPath, const Path& firstDumpPath, const Path& colorizedDumpPath) {
    BinaryInput input;
    Storage lastDump, firstDump;
    Statistics stats;
    Outcome res1 = input.load(lastDumpPath, lastDump, stats);
    Outcome res2 = input.load(firstDumpPath, firstDump, stats);
    if (!res1 || !res2) {
        throw IoError((res1 || res2).error());
    }

    // use last dump to find components
    Array<Size> components;
    Post::findComponents(
        lastDump, 2._f, Post::ComponentFlag::ESCAPE_VELOCITY | Post::ComponentFlag::SORT_BY_MASS, components);

    // "colorize" the flag quantity using the components
    SPH_ASSERT(firstDump.getParticleCnt() == components.size());
    firstDump.getValue<Size>(QuantityId::FLAG) = components.clone();

    // save as new file
    BinaryOutput output(colorizedDumpPath);
    output.dump(firstDump, stats);
}

void extractLr(const Path& inputPath, const Path& outputPath) {
    BinaryInput input;
    Storage storage;
    Statistics stats;
    Outcome res = input.load(inputPath, storage, stats);
    if (!res) {
        throw IoError(res.error());
    }

    // allow using this for storage without masses --> add ad hoc mass if it's missing
    if (!storage.has(QuantityId::MASS)) {
        storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, 1._f);
    }

    Array<Size> components;
    const Size componentCnt =
        Post::findComponents(storage, 1.5_f, Post::ComponentFlag::SORT_BY_MASS, components);
    std::cout << "Component cnt = " << componentCnt << std::endl;

    Array<Size> toRemove;
    for (Size i = 0; i < components.size(); ++i) {
        if (components[i] != 0) {
            // not LR
            toRemove.push(i);
        }
    }
    storage.remove(toRemove, Storage::IndicesFlag::INDICES_SORTED);

    moveToCenterOfMassFrame(storage);

    BinaryOutput output(outputPath);
    output.dump(storage, stats);

    if (storage.has(QuantityId::DENSITY)) {
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);

        Float volume = 0._f;
        for (Size i = 0; i < m.size(); ++i) {
            volume += m[i] / rho[i];
        }

        std::cout << "eq. diameter = " << Sph::cbrt(3._f * volume / (4._f * PI)) * 2._f / 1000._f << "km"
                  << std::endl;
    }

    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    const Float omega = getLength(Post::getAngularFrequency(m, r, v));
    std::cout << "period = " << 2._f * PI / omega / 3600._f << "h" << std::endl;

    SymmetricTensor I = Post::getInertiaTensor(m, r);
    SPH_ASSERT(isReal(I), I);
    Eigen e = eigenDecomposition(I);
    /*std::cout << "I = " << I << std::endl;
    std::cout << "matrix = " << e.vectors << std::endl;
    std::cout << "values = " << e.values << std::endl;*/
    const Float A = e.values[2];
    const Float B = e.values[1];
    const Float C = e.values[0];
    const Float a = sqrt(B + C - A);
    const Float b = sqrt(A + C - B);
    const Float c = sqrt(A + B - C);
    SPH_ASSERT(a > 0._f && b > 0._f && c > 0._f, a, b, c);
    std::cout << "a/b = " << a / b << std::endl;
    std::cout << "b/c = " << b / c << std::endl;
}

void printHelp() {
    std::cout << "Expected usage: post mode [parameters]" << std::endl
              << " where 'mode' is one of:" << std::endl
              << " - pkdgravToSfd   - computes the cumulative SFD from pkdgrav output file" << std::endl
              << " - pkdgravToOmega - computes the spin rate distribution from pkdgrav output file"
              << std::endl
              << " - pkdgravToMoons - finds satellites of the largest remnant (fragment) from pkdgrav "
                 "output file"
              << std::endl
              << "- ssfToSfd - computes the cumulative SFD from SPH output file" << std::endl
              << "- ssfToVelocity - computes the velocity distribution from SPH output file" << std::endl
              << "- harris - TODO" << std::endl
              << "- stats - prints ejected mass and the period of the largest remnant" << std::endl
              << "- swift - makes yarko.in, yorp.in and spin.in input file for swift" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printHelp();
        return 0;
    }
    try {

        String mode = String::fromAscii(argv[1]);
        if (mode == "pkdgravToSfd") {
            if (argc < 4) {
                std::cout << "Expected parameters: post pkdgravToSfd ss.50000.bt sfd.txt";
                return 0;
            }
            return pkdgravToSfd(Path(String::fromAscii(argv[2])), Path(String::fromAscii(argv[3])));
        } else if (mode == "pkdgravToOmega") {
            if (argc < 4) {
                std::cout << "Expected parameters: post pkdgravToOmega ss.50000.bt omega.txt";
                return 0;
            }
            return pkdgravToOmega(Path(String::fromAscii(argv[2])), Path(String::fromAscii(argv[3])));
        } else if (mode == "pkdgravToMoons") {
            if (argc < 4) {
                std::cout << "Expected parameters: post pkdgravToMoons ss.50000.bt 0.1";
                return 0;
            }
            const Float limit = std::atof(argv[3]);
            return pkdgravToMoons(Path(String::fromAscii(argv[2])), limit);
        } else if (mode == "ssfToSfd") {
            if (argc < 4) {
                std::cout << "Expected parameters: post ssfToSfd [--components] output.ssf sfd.txt";
                return 0;
            }
            if (String(String::fromAscii(argv[2])) == "--components") {
                return ssfToSfd(Post::HistogramSource::COMPONENTS,
                    Path(String::fromAscii(argv[3])),
                    Path(String::fromAscii(argv[4])));
            } else {
                return ssfToSfd(Post::HistogramSource::PARTICLES,
                    Path(String::fromAscii(argv[2])),
                    Path(String::fromAscii(argv[3])));
            }
        } else if (mode == "ssfToVelocity") {
            return ssfToVelocity(Path(String::fromAscii(argv[2])), Path(String::fromAscii(argv[3])));
        } else if (mode == "ssfToOmega") {
            if (argc < 6) {
                std::cout << "Expected parameters: post ssfToOmega output.ssf omega.txt omega_D.txt "
                             "omega_dir.txt";
                return 0;
            }
            return ssfToOmega(Path(String::fromAscii(argv[2])),
                Path(String::fromAscii(argv[3])),
                Path(String::fromAscii(argv[4])),
                Path(String::fromAscii(argv[5])));
        } else if (mode == "ssfToVelDir") {
            if (argc < 4) {
                std::cout << "Expected parameters: post ssfToVelDir output.ssf veldir.txt" << std::endl;
                return 0;
            }
            ssfToVelDir(Path(String::fromAscii(argv[2])), Path(String::fromAscii(argv[3])));
        } else if (mode == "harris") {
            processHarrisFile();
        } else if (mode == "swift") {
            if (argc < 3) {
                std::cout << "Expected parameters: post maketp D.dat";
                return 0;
            }
            makeSwift(Path(String::fromAscii(argv[2])));
        } else if (mode == "origComponents") {
            if (argc < 5) {
                std::cout << "Expected parameters: post origComponents lastDump.ssf firstDump.ssf "
                             "colorizedDump.ssf"
                          << std::endl;
            }
            origComponents(Path(String::fromAscii(argv[2])),
                Path(String::fromAscii(argv[3])),
                Path(String::fromAscii(argv[4])));
        } else if (mode == "extractLr") {
            if (argc < 4) {
                std::cout << "Expected parameters: post extractLr input.ssf lr.ssf" << std::endl;
            }
            extractLr(Path(String::fromAscii(argv[2])), Path(String::fromAscii(argv[3])));
        } else {
            printHelp();
            return 0;
        }

    } catch (const std::exception& e) {
        std::cout << "ERROR: " << e.what() << std::endl;
        return -1;
    }
}
