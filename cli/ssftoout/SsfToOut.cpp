/// \brief Converts binary .ssf file to human-readable .txt file.

#include "Sph.h"
#include <iostream>

using namespace Sph;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: ssftotxt file.ssf" << std::endl;
        return 0;
    }

    BinaryInput input;
    Storage storage;
    Statistics stats;
    Path inputPath(argv[1]);
    Path outputPath = Path(inputPath).replaceExtension("out");

    Outcome outcome = input.load(inputPath, storage, stats);
    if (outcome) {
        std::cout << "Success" << std::endl;
    } else {
        std::cout << "Cannot load binary file:" << std::endl << outcome.error() << std::endl;
        return -1;
    }

    PkdgravOutput output(outputPath, PkdgravParams{});
    try {
        output.dump(storage, stats);
    } catch (std::exception& e) {
        std::cout << "Cannot save text file: " << std::endl << e.what() << std::endl;
        return -2;
    }

    return 0;
}
