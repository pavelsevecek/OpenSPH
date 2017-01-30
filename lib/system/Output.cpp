#include "system/Output.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

static void printHeader(std::ofstream& ofs, const QuantityIds key, Quantity& q) {
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
        case ValueEnum::TENSOR:
            ofs << std::setw(15) << (name + " [xx]") << std::setw(15) << (name + " [yy]") << std::setw(15)
                << (name + " [zz]") << std::setw(15) << (name + " [xy]") << std::setw(15) << (name + " [xz]")
                << std::setw(15) << (name + " [yz]");
            break;
        case ValueEnum::TRACELESS_TENSOR:
            ofs << std::setw(15) << (name + " [xx]") << std::setw(15) << (name + " [yy]") << std::setw(15)
                << (name + " [xy]") << std::setw(15) << (name + " [xz]") << std::setw(15) << (name + " [yz]");
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

TextOutput::TextOutput(const std::string& fileMask, const std::string& runName, Array<QuantityIds>&& columns)
    : Abstract::Output(fileMask)
    , runName(runName)
    , columns(std::move(columns)) {}

struct LinePrinter {
    template <typename TValue>
    void visit(Quantity& q, const Size i, std::ofstream& ofs) {
        if (q.getOrderEnum() == OrderEnum::SECOND_ORDER) {
            ofs << std::setprecision(6) << std::setw(15) << q.getValue<TValue>()[i] << std::setw(15)
                << q.getDt<TValue>()[i];
        } else {
            ofs << std::setprecision(6) << std::setw(15) << q.getValue<TValue>()[i];
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
    for (QuantityIds key : columns) {
        printHeader(ofs, key, storage.getQuantity(key));
    }
    ofs << std::endl;

    // print data lines, starting with second-order quantities
    for (Size i = 0; i < storage.getParticleCnt(); ++i) {
        for (QuantityIds key : columns) {
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

BinaryOutput::BinaryOutput(const std::string& fileMask, const std::string& runName)
    : Abstract::Output(fileMask)
    , runName(runName) {}

struct StoreBuffers {
    template <typename Value>
    void visit(std::ofstream& ofs, Quantity& q) {}
};

template <typename TValue, typename TStoreValue>
static void storeBuffers(Quantity& q, TStoreValue&& storeValue) {
    StaticArray<Array<TValue>&, 3> buffers = q.getBuffers<TValue>();
    switch (q.getOrderEnum()) {
    case OrderEnum::ZERO_ORDER:
    case OrderEnum::FIRST_ORDER:
        for (Size i = 0; i < q.size(); ++i) {
            storeValue(buffers[0][i]);
        }
        break;
    case OrderEnum::SECOND_ORDER:
        for (Size i = 0; i < q.size(); ++i) {
            storeValue(buffers[0][i]); // store value
        }
        for (Size i = 0; i < q.size(); ++i) {
            storeValue(buffers[1][i]); // store derivative
        }
    default:
        STOP;
    }
}

std::string BinaryOutput::dump(Storage& storage, const Float time) {
    const std::string fileName = getFileName();
    std::ofstream ofs(fileName);
    // file format identifie
    ofs << "SPH" << time << storage.getParticleCnt() << storage.getQuantityCnt();
    // storage dump
    for (auto& i : storage) {
        // first 3 values: quantity ID, order (number of derivatives), type
        Quantity& q = i.second;
        ofs << Size(i.first) << Size(q.getOrderEnum()) << Size(q.getValueEnum());
        switch (q.getValueEnum()) {
        case ValueEnum::INDEX:
            storeBuffers<Size>(q, [&ofs](const Size idx) { ofs << idx; });
            break;
        case ValueEnum::SCALAR:
            storeBuffers<Float>(q, [&ofs](const Float f) { ofs << f; });
            break;
        /// \todo storing smoothing length?
        case ValueEnum::VECTOR:
            storeBuffers<Vector>(q, [&ofs](const Vector& v) { ofs << v[X] << v[Y] << v[Z]; });
            break;
        case ValueEnum::TENSOR:
            storeBuffers<Tensor>(q, [&ofs](const Tensor& t) {
                ofs << t(0, 0) << t(1, 1) << t(2, 2) << t(0, 1) << t(0, 2) << t(1, 2);
            });
            break;
        case ValueEnum::TRACELESS_TENSOR:
            storeBuffers<TracelessTensor>(q, [&ofs](const TracelessTensor& t) {
                ofs << t(0, 0) << t(1, 1) << t(0, 1) << t(0, 2) << t(1, 2);
            });
            break;
        }
    }
    ofs.close();
    return fileName;
}

Outcome BinaryOutput::load(const std::string& path, Storage& storage) {
    storage.removeAll();
    std::ifstream ifs(path);
    char identifier[4];
    ifs.read(identifier, 4);
    if (std::string(identifier) != "SPH") {
        return "Invalid file format";
    }
    Float time;
    Size particleCnt, quantityCnt;
    ifs >> time >> particleCnt >> quantityCnt;
    ifs.close();
    return true;
}

NAMESPACE_SPH_END
