#include "io/Logger.h"
#include "io/Output.h"
#include "io/Path.h"
#include "post/Analysis.h"
#include "quantities/Storage.h"
#include "run/Collision.h"
#include <fstream>
#include <iostream>

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
    Post::HistogramParams params;
    params.source = Post::HistogramParams::Source::PARTICLES;
    params.id = Post::HistogramId::ANGULAR_VELOCITIES;
    params.binCnt = 50;
    params.validator = [](const Float value) { return value > 0._f; };

    Array<Post::SfdPoint> sfd = Post::getDifferentialSfd(storage.value(), params);
    FileLogger logOmegaSfd(omegaPath, FileLogger::Options::KEEP_OPENED);
    for (Post::SfdPoint& p : sfd) {
        logOmegaSfd.write(p.value, "  ", p.count);
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
    /// \todo use correct radius here, we assume that close ecounters will eventually collide
    Array<Post::MoonEnum> moons = Post::findMoons(storage.value(), 2._f, limit);
    Size moonCnt = std::count(moons.begin(), moons.end(), Post::MoonEnum::MOON);
    std::cout << "Moon count = " << moonCnt << std::endl;
    return 0;
}

int sphToSfd(const Path& filePath, const Path& settingsPath, const Path& sfdPath) {
    std::cout << "Processing SPH file ... " << std::endl;
    TextOutput output;
    setupCollisionColumns(output);

    Storage storage;
    Outcome outcome = output.load(filePath, storage);
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
    tie(r, v) = storage.getAll<Vector>(QuantityId::POSITIONS);
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

void processHarrisFile() {
    Path harrisPath("/home/pavel/projects/astro/asteroids/grant3/harris.out");
    std::ifstream ifs(harrisPath.native());
    while (ifs) {
        std::string dummy;
        ifs >> dummy >> dummy;

        std::string name;
        ifs >> name;
        for (int i = 0; i < 6; ++i) {
            ifs >> dummy;
        }

        float radius;
        ifs >> radius;
        for (int i = 0; i < 5; ++i) {
            ifs >> dummy;
        }

        float period;
        ifs >> period;
        for (int i = 0; i < 10; ++i) {
            ifs >> dummy;
        }

        if (radius > 0.1f && period < 0.1f) {
            std::cout << name << " - " << radius << " km, P = " << period << std::endl;
        }
    }
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
              << "- sphToSfd - computes the cummulative SFD from SPH output file" << std::endl;
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
    } else {
        printHelp();
        return 0;
    }
}
