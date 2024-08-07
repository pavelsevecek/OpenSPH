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
    Path inputPath(String::fromAscii(argv[1]));
    Path outputPath = Path(inputPath).replaceExtension("txt");

    Outcome outcome = input.load(inputPath, storage, stats);
    if (!outcome) {
        std::cout << "Cannot load binary file:" << std::endl << outcome.error() << std::endl;
        return -1;
    }

    TextOutput output(outputPath, "ssftotxt", EMPTY_FLAGS, TextOutput::Options::DUMP_ALL);
    try {
        output.dump(storage, stats);
    } catch (const std::exception& e) {
        std::cout << "Cannot save text file: " << std::endl << e.what() << std::endl;
        return -2;
    }

    std::cout << "Data written to '" << outputPath.string() << "'" << std::endl;

    return 0;
}
