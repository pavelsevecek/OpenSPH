#include "io/Logger.h"
#include "io/Output.h"
#include "io/Path.h"
#include "post/Analysis.h"
#include "quantities/Storage.h"
#include "run/Collision.h"
#include <iostream>

using namespace Sph;

int processPkdgravFile(const Path& filePath, const Path& sfdPath) {
    std::cout << "Processing pkdgrav file ... " << std::endl;
    Expected<Storage> storage = Post::parsePkdgravOutput(filePath);
    if (!storage) {
        std::cout << "Invalid file: " << storage.error() << std::endl;
        return 0;
    }
    Array<Post::SfdPoint> sfd = Post::getCummulativeSfd(storage.value(), {});
    FileLogger logSfd(sfdPath, FileLogger::Options::KEEP_OPENED);
    for (Post::SfdPoint& p : sfd) {
        logSfd.write(p.radius, "  ", p.count);
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
        logSfd.write(p.radius, "  ", p.count);
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Expected usage: post inputFile outputFile" << std::endl;
        return 0;
    }
    Path path(argv[1]);
    if (path.extension() == Path("50000.bt")) {
        return processPkdgravFile(path, Path(argv[2]));
    } else if (path.extension() == Path("txt")) {
        if (argc < 4) {
            std::cout << "Missing settings file" << std::endl;
            return 0;
        }
        return processSphFile(path, Path(argv[2]), Path(argv[3]));
    }
}
