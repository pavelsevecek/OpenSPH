#include "io/Logger.h"
#include "io/Path.h"
#include "post/Analysis.h"
#include "quantities/Storage.h"
#include <iostream>

using namespace Sph;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Expected usage: post inputFile outputFile" << std::endl;
        return 0;
    }
    Path path(argv[1]);
    if (path.extension() == Path("50000.bt")) {
        std::cout << "Processing pkdgrav file ... " << std::endl;
        Expected<Storage> storage = Post::parsePkdgravOutput(path);
        if (!storage) {
            std::cout << "Invalid file: " << storage.error() << std::endl;
            return 0;
        }
        Array<Post::SfdPoint> sfd = Post::getCummulativeSfd(storage.value(), {});
        FileLogger logSfd(Path(argv[2]), FileLogger::Options::KEEP_OPENED);
        for (Post::SfdPoint& p : sfd) {
            logSfd.write(p.radius, "  ", p.count);
        }
    } else if (path.extension() == Path("txt")) {
        std::cout << "Processing SPH file ... " << std::endl;
    }
    return 0;
}
