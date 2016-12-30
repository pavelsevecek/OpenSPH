#include "system/Output.h"

NAMESPACE_SPH_BEGIN

static void printHeader(std::ofstream& ofs, const QuantityKey key, Quantity& q) {
    auto printHeaderImpl = [&q, &ofs](const std::string& name) {
        switch (q.getValueEnum()) {
        case ValueEnum::SCALAR:
        case ValueEnum::INDEX:
            ofs << std::setw(15) << name;
            break;
        case ValueEnum::VECTOR:
            ofs << std::setw(15) << (name + " [x]") << std::setw(15) << (name + " [y]") << std::setw(15)
                << (name + " [z]");
            break;
        default:
            NOT_IMPLEMENTED;
        }
    };
    switch (q.getOrderEnum()) {
    case OrderEnum::SECOND_ORDER:
        printHeaderImpl(getQuantityName(key));
        printHeaderImpl(getDerivativeName(key));
        break;
    case OrderEnum::FIRST_ORDER:
    case OrderEnum::ZERO_ORDER:
        printHeaderImpl(getQuantityName(key));
        break;
    default:
        STOP;
    }
}

TextOutput::TextOutput(const std::string& fileMask, const std::string& runName, Array<QuantityKey>&& columns)
    : Abstract::Output(fileMask)
    , runName(runName)
    , columns(std::move(columns)) {}

std::string TextOutput::dump(Storage& storage, const Float time) {
    const std::string fileName = getFileName();
    std::ofstream ofs(fileName);
    // print description
    ofs << "# Run: " << runName << std::endl;
    ofs << "# SPH dump, time = " << time << std::endl;
    ofs << "# ";
    for (QuantityKey key : columns) {
        printHeader(ofs, key, storage.getQuantity(key));
    }
    ofs << std::endl;

    // print data lines, starting with second-order quantities
    for (int i = 0; i < storage.getParticleCnt(); ++i) {
        iterateCustom<VisitorEnum::SECOND_ORDER>(
            storage, columns, [i, &ofs](auto&& v, auto&& dv, auto&& UNUSED(d2v)) {
                ofs << std::setw(15) << v[i] << std::setw(15) << dv[i];
            });
        iterateCustom<VisitorEnum::FIRST_ORDER>(
            storage, columns, [i, &ofs](auto&& v, auto&& UNUSED(dv)) { ofs << std::setw(15) << v[i]; });
        iterateCustom<VisitorEnum::ZERO_ORDER>(
            storage, columns, [i, &ofs](auto&& v) { ofs << std::setw(15) << v[i]; });
        ofs << std::endl;
    }
    ofs.close();
    this->dumpNum++;
    return fileName;
}

std::string GnuplotOutput::dump(Storage& storage, const Float time) {
    const std::string fileName = TextOutput::dump(storage, time);
    const std::string nameWithoutExt = fileName.substr(0, fileName.find_last_of("."));
    const std::string command =
        "gnuplot -e \"filename='" + nameWithoutExt + "'; time=" + std::to_string(time) + "\" " + scriptPath;
    system(command.c_str());
    return fileName;
}

NAMESPACE_SPH_END
