#include "io/Logger.h"
#include "io/Path.h"
#include "post/Analysis.h"
#include "quantities/Storage.h"
#include <iostream>

using namespace Sph;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Expected usage: post pathToPkdgravFile outputFileName" << std::endl;
        return 0;
    }
    Path path(argv[1]);
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
    return 0;
}
