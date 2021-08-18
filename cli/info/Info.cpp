/// \brief Executable showing basic information about .ssf file

#include "Sph.h"
#include "io/Table.h"

using namespace Sph;

int printBinaryFileInfo(ILogger& logger, const Path& path) {
    BinaryInput input;
    Expected<BinaryInput::Info> info = input.getInfo(path);
    if (!info) {
        logger.write("Cannot obtain file info from '", path.native(), "'");
        return -1;
    }

    Size row = 0;
    Table table(3);
    table.setCell(0, row, "File name:");
    table.setCell(1, row, path.fileName().native());
    ++row;
    table.setCell(0, row, "File version:");
    table.setCell(1, row, std::to_string(int(info->version)));
    ++row;
    table.setCell(0, row, "Particle count:");
    table.setCell(1, row, std::to_string(info->particleCnt));
    ++row;
    if (info->version >= BinaryIoVersion::V2021_08_08) {
        table.setCell(0, row, "Attractor count:");
        table.setCell(1, row, std::to_string(info->attractorCnt));
        ++row;
    }
    table.setCell(0, row, "Material count:");
    table.setCell(1, row, std::to_string(info->materialCnt));
    ++row;
    table.setCell(0, row, "Quantity count:");
    table.setCell(1, row, std::to_string(info->quantityCnt));
    ++row;
    table.setCell(0, row, "Run time:");
    table.setCell(1, row, std::to_string(info->runTime));
    ++row;
    table.setCell(0, row, "Time step:");
    table.setCell(1, row, std::to_string(info->timeStep));
    ++row;
    table.setCell(0, row, "Wallclock time:");
    table.setCell(1, row, getFormattedTime(info->wallclockTime));
    ++row;
    table.setCell(0, row, "Run type:");
    if (info->runType) {
        table.setCell(1, row, EnumMap::toString<RunTypeEnum>(info->runType.value()));
    } else {
        table.setCell(1, row, "unknown");
    }
    ++row;
    table.setCell(0, row, "Build date:");
    if (info->buildDate) {
        table.setCell(1, row, info->buildDate.value());
    } else {
        table.setCell(1, row, "unknown");
    }
    ++row;
    logger.write(table.toString());
    return 0;
}

int printCompressedFileInfo(ILogger& logger, const Path& path) {
    CompressedInput input;
    Expected<CompressedInput::Info> info = input.getInfo(path);
    if (!info) {
        logger.write("Cannot obtain file info from '", path.native(), "'");
        return -1;
    }

    Size row = 0;
    Table table(3);
    table.setCell(0, row, "File name:");
    table.setCell(1, row, path.fileName().native());
    ++row;
    table.setCell(0, row, "File version:");
    table.setCell(1, row, std::to_string(int(info->version)));
    ++row;
    table.setCell(0, row, "Particle count:");
    table.setCell(1, row, std::to_string(info->particleCnt));
    ++row;
    if (info->version >= CompressedIoVersion::V2021_08_08) {
        table.setCell(0, row, "Attractor count:");
        table.setCell(1, row, std::to_string(info->attractorCnt));
        ++row;
    }
    table.setCell(0, row, "Run time:");
    table.setCell(1, row, std::to_string(info->runTime));
    ++row;
    table.setCell(0, row, "Run type:");
    table.setCell(1, row, EnumMap::toString<RunTypeEnum>(info->runType));
    logger.write(table.toString());
    return 0;
}

int main(int argc, char* argv[]) {
    StdOutLogger logger;
    if (argc != 2 || argv[1] == std::string("--help")) {
        logger.write("Usage: opensph-info file");
        return 0;
    }

    const Optional<IoEnum> type = getIoEnum(Path(argv[1]).extension().native());
    switch (type.valueOr(IoEnum::NONE)) {
    case IoEnum::BINARY_FILE:
        return printBinaryFileInfo(logger, Path(argv[1]));
    case IoEnum::DATA_FILE:
        return printCompressedFileInfo(logger, Path(argv[1]));
    default:
        logger.write("Unknown file format.");
        return -1;
    }
}
