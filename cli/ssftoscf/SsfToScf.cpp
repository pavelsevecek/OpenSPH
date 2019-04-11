/// \brief Converts binary .ssf file to compressed .scf

#include "Sph.h"
#include <iostream>

using namespace Sph;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: ssftoscf file.ssf" << std::endl;
        return 0;
    }

    BinaryInput input;
    Storage storage;
    Statistics stats;
    Path inputPath(argv[1]);
    Expected<BinaryInput::Info> info = input.getInfo(inputPath);
    if (!info) {
        std::cout << "Cannot get binary file info: " << std::endl << info.error() << std::endl;
        return -1;
    }

    Outcome outcome = input.load(inputPath, storage, stats);
    if (outcome) {
        std::cout << "Success" << std::endl;
    } else {
        std::cout << "Cannot load binary file:" << std::endl << outcome.error() << std::endl;
        return -1;
    }

    Path outputPath = inputPath.replaceExtension("scf");
    CompressedOutput output(outputPath, CompressionEnum::NONE, info->runType.valueOr(RunTypeEnum::SPH));
    try {
        output.dump(storage, stats);
    } catch (std::exception& e) {
        std::cout << "Cannot save compressed file: " << std::endl << e.what() << std::endl;
        return -2;
    }

    return 0;
}
