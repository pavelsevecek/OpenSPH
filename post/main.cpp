#include "io/FileManager.h"
#include "io/FileSystem.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "objects/utility/StringUtils.h"
#include "post/Analysis.h"
#include "post/StatisticTests.h"
#include "quantities/Storage.h"
#include "run/Collision.h"
#include "system/Process.h"
#include "system/Statistics.h"
#include "thread/Pool.h"
#include <fstream>
#include <iostream>
#include <mutex>

using namespace Sph;

int pkdgravToSfd(const Path& filePath, const Path& sfdPath) {
    std::cout << "Processing pkdgrav file ... " << std::endl;
    Expected<Storage> storage = Post::parsePkdgravOutput(filePath);
    if (!storage) {
        std::cout << "Invalid file: " << storage.error() << std::endl;
        return 0;
    }
    Post::HistogramParams params;
    params.source = Post::HistogramParams::Source::PARTICLES;
    params.id = Post::HistogramId::RADII;
    Array<Post::SfdPoint> sfd = Post::getCummulativeSfd(storage.value(), params);
    FileLogger logRadiiSfd(sfdPath, FileLogger::Options::KEEP_OPENED);
    for (Post::SfdPoint& p : sfd) {
        logRadiiSfd.write(p.value, "  ", p.count);
    }
    return 0;
}

int pkdgravToOmega(const Path& filePath, const Path& omegaPath) {
    std::cout << "Processing pkdgrav file ... " << std::endl;
    Expected<Storage> storage = Post::parsePkdgravOutput(filePath);
    if (!storage) {
        std::cout << "Invalid file: " << storage.error() << std::endl;
        return 0;
    }
    /*Post::HistogramParams params;
    params.source = Post::HistogramParams::Source::PARTICLES;
    params.id = Post::HistogramId::ANGULAR_VELOCITIES;
    params.binCnt = 50;
    params.validator = [](const Float value) { return value > 0._f; };*/

    // Array<Post::SfdPoint> sfd = Post::getDifferentialSfd(storage.value(), params);
    FileLogger logOmegaSfd(omegaPath, FileLogger::Options::KEEP_OPENED);
    /*for (Post::SfdPoint& p : sfd) {
        logOmegaSfd.write(p.value, "  ", p.count);
    }*/
    ArrayView<Vector> omega = storage->getValue<Vector>(QuantityId::ANGULAR_VELOCITY);
    std::sort(
        omega.begin(), omega.end(), [](Vector& v1, Vector& v2) { return getLength(v1) > getLength(v2); });
    for (Vector v : omega) {
        if (getLength(v) != 0._f) {
            logOmegaSfd.write(getLength(v));
        }
    }
    return 0;
}

int pkdgravToMoons(const Path& filePath, const float limit) {
    std::cout << "Processing pkdgrav file ... " << std::endl;
    Expected<Storage> storage = Post::parsePkdgravOutput(filePath);
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

int sphToSfd(const Path& filePath, const Path& settingsPath, const Path& sfdPath) {
    std::cout << "Processing SPH file ... " << std::endl;
    TextOutput output(TextOutput::Options::EXTENDED_COLUMNS);

    Storage storage;
    Statistics stats;
    Outcome outcome = output.load(filePath, storage, stats);
    if (!outcome) {
        std::cout << "Cannot load particle data, " << outcome.error() << std::endl;
        return 0;
    }

    RunSettings settings;
    outcome = settings.loadFromFile(settingsPath);
    if (!outcome) {
        std::cout << "Cannot load settings, " << outcome.error() << std::endl;
        return 0;
    }
    const Vector omega = settings.get<Vector>(RunSettingsId::FRAME_ANGULAR_FREQUENCY);
    std::cout << "Adding rotation of P = " << Vector(2._f * PI) / omega << std::endl;

    ArrayView<Vector> r, v;
    tie(r, v) = storage.getAll<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        v[i] += cross(omega, r[i]);
    }

    // ad-hoc value of particle radius (0.3h)
    StdOutLogger logger;
    Storage bodies = Post::findFutureBodies(storage, 1._f, logger);

    Post::HistogramParams params;
    params.source = Post::HistogramParams::Source::PARTICLES;
    Array<Post::SfdPoint> sfd = Post::getCummulativeSfd(bodies, params);
    FileLogger logSfd(sfdPath, FileLogger::Options::KEEP_OPENED);
    for (Post::SfdPoint& p : sfd) {
        logSfd.write(p.value, "  ", p.count);
    }
    return 0;
}

struct HarrisAsteroid {
    Optional<Size> number;
    std::string name;
    Optional<float> radius;
    Optional<float> period;
};

static Array<HarrisAsteroid> loadHarris(std::ifstream& ifs) {
    Array<HarrisAsteroid> harris;
    while (ifs) {
        std::string dummy;
        std::string number;
        ifs >> number >> dummy;

        std::string name;
        ifs >> name;
        for (int i = 0; i < 6; ++i) {
            ifs >> dummy;
        }

        std::string radius;
        ifs >> radius;
        for (int i = 0; i < 5; ++i) {
            ifs >> dummy;
        }

        std::string period;
        ifs >> period;
        for (int i = 0; i < 10; ++i) {
            ifs >> dummy;
        }

        harris.push(HarrisAsteroid{
            fromString<Size>(number),
            name,
            fromString<float>(radius),
            fromString<float>(period),
        });
    }
    return harris;
}

struct FamilyAsteroid {
    Optional<Size> number;
    Optional<std::string> name;
};

static Array<FamilyAsteroid> loadFamilies(std::ifstream& ifs) {
    ASSERT(ifs);
    Array<FamilyAsteroid> asteroids;
    std::string line;
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
        std::stringstream ss(line);
        // both formats start with asteroid number
        std::string number;
        ss >> number;
        if (format == 2) {
            std::string name;
            ss >> name;
            asteroids.push(FamilyAsteroid{ fromString<Size>(number), name });
        } else {
            asteroids.push(FamilyAsteroid{ fromString<Size>(number), NOTHING });
        }
        // check that we have at least one information
        ASSERT(asteroids.back().name || asteroids.back().number);
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

    std::ifstream ifs(familyData.native());
    ASSERT(ifs);
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
    if (found.size() < 4) {
        // too few data to do any statistics
        return NOTHING;
    }

    FileSystem::createDirectory(outputPath.parentPath());
    std::ofstream ofs(outputPath.native());
    ASSERT(ofs);

    auto compare = [](HarrisAsteroid& ast1, HarrisAsteroid& ast2) {
        return ast1.radius.value() < ast2.radius.value();
    };
    Iterator<HarrisAsteroid> largestRemnant = std::max_element(found.begin(), found.end(), compare);

    auto periodToOmega = [](const Float P) { return 1._f / (P / 24._f); };

    Array<PlotPoint> points;
    for (HarrisAsteroid& ast : found) {
        const Float omega = periodToOmega(ast.period.value());
        if (&ast != &*largestRemnant) {
            ofs << ast.radius.value() << "  " << omega << std::endl;
        }
        points.push(PlotPoint(ast.radius.value(), omega));
    }
    Interval rangeOmega;
    rangeOmega.extend(periodToOmega(rangePeriod.lower()));
    rangeOmega.extend(periodToOmega(rangePeriod.upper()));

    Post::KsFunction uniformCdf = Post::getUniformKsFunction(rangeR, rangeOmega);
    Post::KsResult result = Post::kolmogorovSmirnovTest(points, uniformCdf);

    outPoints = std::move(points);
    return result;
}

static std::string elliptize(const std::string& s, const Size maxSize) {
    ASSERT(maxSize >= 5);
    if (s.size() < maxSize) {
        return s;
    }
    const Size cutSize = (maxSize - 3) / 2;
    return s.substr(0, cutSize) + "..." + s.substr(s.size() - cutSize);
}

void processHarrisFile() {
    Path harrisPath("/home/pavel/projects/astro/asteroids/grant3/harris.out");
    std::ifstream ifs(harrisPath.native());
    Array<HarrisAsteroid> harris = loadHarris(ifs);
    ifs.close();

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

    parallelFor(ThreadPool::getGlobalInstance(), 0, paths.size(), [&](const Size index) {
        {
            std::unique_lock<std::mutex> lock(printMutex);
            std::cout << paths[index].native() << std::endl;
        }
        const Path name = paths[index].fileName();
        const Path targetPath = Path("D_omega") / Path(name.native() + ".txt");
        const Path ksPath = Path("KS") / Path(name.native() + ".txt");
        Array<PlotPoint> points;
        Optional<Post::KsResult> ks = printDvsOmega(parentPath / paths[index], targetPath, harris, points);

        std::unique_lock<std::mutex> lock(plotMutex);
        if (!FileSystem::pathExists(targetPath) || FileSystem::fileSize(targetPath) == 0) {
            return;
        }
        if (ks) {
            kss << std::left << std::setw(35) << elliptize(Path(name).removeExtension().native(), 30)
                << std::setw(15) << ks->D << std::setw(15) << ks->prob << std::endl;
        }
        for (PlotPoint p : points) {
            alls << p.x << "  " << p.y << "  " << index << std::endl;
        }

        FileSystem::copyFile(targetPath, Path("family.txt"));
        // make plot
        Process gnuplot(Path("/bin/gnuplot"), { "doplot.plt" });
        gnuplot.wait();
        ASSERT(FileSystem::pathExists(Path("plot.png")));
        FileSystem::copyFile(Path("plot.png"), uniquePaths.getPath(Path(targetPath).replaceExtension("png")));
    });

    FileSystem::copyFile(Path("D_omega_all.txt"), Path("family.txt"));
    Process gnuplot(Path("/bin/gnuplot"), { "doplot_all.plt" });
    gnuplot.wait();
}

void printHelp() {
    std::cout << "Expected usage: post mode [parameters]" << std::endl
              << " where 'mode' is one of:" << std::endl
              << " - pkdgravToSfd   - computes the cummulative SFD from pkdgrav output file" << std::endl
              << " - pkdgravToOmega - computes the spin rate distribution from pkdgrav output file"
              << std::endl
              << " - pkdgravToMoons - finds satellites of the largest remnant (fragment) from pkdgrav "
                 "output file"
              << std::endl
              << "- sphToSfd - computes the cummulative SFD from SPH output file" << std::endl
              << "- harris - TODO" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printHelp();
        return 0;
    }
    std::string mode(argv[1]);
    if (mode == "pkdgravToSfd") {
        if (argc < 4) {
            std::cout << "Expected parameters: post pkdgravToSfd ss.50000.bt sfd.txt";
            return 0;
        }
        return pkdgravToSfd(Path(argv[2]), Path(argv[3]));
    } else if (mode == "pkdgravToOmega") {
        if (argc < 4) {
            std::cout << "Expected parameters: post pkdgravToOmega ss.50000.bt omega.txt";
            return 0;
        }
        return pkdgravToOmega(Path(argv[2]), Path(argv[3]));
    } else if (mode == "pkdgravToMoons") {
        if (argc < 4) {
            std::cout << "Expected parameters: post pkdgravToMoons ss.50000.bt 0.1";
            return 0;
        }
        const float limit = std::atof(argv[3]);
        return pkdgravToMoons(Path(argv[2]), limit);
    } else if (mode == "sphToSfd") {
        if (argc < 5) {
            std::cout << "Expected parameters: post sphToSfd out_0090.txt code.sph sfd.txt";
            return 0;
        }
        return sphToSfd(Path(argv[2]), Path(argv[3]), Path(argv[4]));
    } else if (mode == "harris") {
        processHarrisFile();
    } else {
        printHelp();
        return 0;
    }
}
