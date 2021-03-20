/// \brief Executable showing basic information about .ssf file

#include "Sph.h"
#include "io/Table.h"

using namespace Sph;

int main(int argc, char* argv[]) {
    StdOutLogger logger;
    if (argc != 2) {
        logger.write("Usage: opensph-info file.ssf");
        return 0;
    }
    BinaryInput input;
    Expected<BinaryInput::Info> info = input.getInfo(Path(argv[1]));
    if (!info) {
        logger.write("Cannot obtain file info from '", argv[1], "'");
        return -1;
    }

    Size row = 0;
    Table table(3);
    table.setCell(0, row, "File name:");
    table.setCell(1, row, Path(argv[1]).fileName().native());
    ++row;
    table.setCell(0, row, "File version:");
    table.setCell(1, row, std::to_string(int(info->version)));
    ++row;
    table.setCell(0, row, "Particle count:");
    table.setCell(1, row, std::to_string(info->particleCnt));
    ++row;
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
