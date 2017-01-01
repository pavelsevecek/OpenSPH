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

struct LinePrinter {
    template <typename TValue>
    void visit(Quantity& q, const Size i, std::ofstream& ofs) {
        if (q.getOrderEnum() == OrderEnum::SECOND_ORDER) {
            ofs << std::fixed << std::setprecision(6) << std::setw(15) << q.getValue<TValue>()[i]
                << std::setw(15) << q.getDt<TValue>()[i];
        } else {
            ofs << std::fixed << std::setprecision(6) << std::setw(15) << q.getValue<TValue>()[i];
        }
    }
};

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
    for (Size i = 0; i < storage.getParticleCnt(); ++i) {
        for (QuantityKey key : columns) {
            Quantity& q = storage.getQuantity(key);
            dispatch(q.getValueEnum(), LinePrinter(), q, i, ofs);
        }
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
    const int returned = system(command.c_str());
    (void)returned;
    return fileName;
}

NAMESPACE_SPH_END
