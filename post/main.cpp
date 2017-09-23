#include "io/Logger.h"
#include "io/Output.h"
#include "io/Path.h"
#include "post/Analysis.h"
#include "quantities/Storage.h"
#include "run/Collision.h"
#include <fstream>
#include <iostream>

using namespace Sph;

int processPkdgravFile(const Path& filePath, const Path& sfdPath, const Path& omegaPath) {
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

    params.id = Post::HistogramId::ANGULAR_VELOCITIES;
    params.binCnt = 50;
    struct Validator : public Post::HistogramValidator {
        virtual bool include(const Float& value) const override {
            return value > 0._f;
        }
    };
    params.validator = makeAuto<Validator>();
    sfd = Post::getDifferentialSfd(storage.value(), params);
    FileLogger logOmegaSfd(omegaPath, FileLogger::Options::KEEP_OPENED);
    for (Post::SfdPoint& p : sfd) {
        logOmegaSfd.write(p.value, "  ", p.count);
    }
    return 0;
}

int processSphFile(const Path& filePath, const Path& settingsPath, const Path& sfdPath) {
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

int main(int argc, char** argv) {
    if (true) {
        processHarrisFile();
        return 0;
    }

    if (argc < 4) {
        std::cout << "Expected usage: post inputFile outputFile" << std::endl;
        return 0;
    }
    Path path(argv[1]);
    if (path.extension() == Path("50000.bt")) {
        return processPkdgravFile(path, Path(argv[2]), Path(argv[3]));
    } else if (path.extension() == Path("txt")) {
        if (argc < 4) {
            std::cout << "Missing settings file" << std::endl;
            return 0;
        }
        return processSphFile(path, Path(argv[2]), Path(argv[3]));
    }
}
