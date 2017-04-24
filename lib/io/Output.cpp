﻿#include "io/Output.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

static void printHeader(std::ofstream& ofs, const std::string& name, const ValueEnum type) {
    switch (type) {
    case ValueEnum::SCALAR:
    case ValueEnum::INDEX:
        ofs << std::setw(20) << name;
        break;
    case ValueEnum::VECTOR:
        ofs << std::setw(20) << (name + " [x]") << std::setw(20) << (name + " [y]") << std::setw(20)
            << (name + " [z]");
        break;
    case ValueEnum::TENSOR:
        ofs << std::setw(20) << (name + " [xx]") << std::setw(20) << (name + " [yy]") << std::setw(20)
            << (name + " [zz]") << std::setw(20) << (name + " [xy]") << std::setw(20) << (name + " [xz]")
            << std::setw(20) << (name + " [yz]");
        break;
    case ValueEnum::TRACELESS_TENSOR:
        ofs << std::setw(20) << (name + " [xx]") << std::setw(20) << (name + " [yy]") << std::setw(20)
            << (name + " [xy]") << std::setw(20) << (name + " [xz]") << std::setw(20) << (name + " [yz]");
        break;
    default:
        NOT_IMPLEMENTED;
    }
}

TextOutput::TextOutput(const std::string& fileMask, const std::string& runName, const Flags<Options> flags)
    : Abstract::Output(fileMask)
    , runName(runName)
    , flags(flags) {}

std::string TextOutput::dump(Storage& storage, const Float time) {
    ASSERT(!elements.empty() && "nothing to dump");
    const std::string fileName = paths.getNextPath();
    std::ofstream ofs(fileName);
    // print description
    ofs << "# Run: " << runName << std::endl;
    ofs << "# SPH dump, time = " << time << std::endl;
    ofs << "# ";
    for (auto& element : elements) {
        printHeader(ofs, element->getName(), element->getType());
    }
    ofs << std::endl;

    // print data lines, starting with second-order quantities
    for (Size i = 0; i < storage.getParticleCnt(); ++i) {
        for (auto& element : elements) {
            // write one extra space to be sure numbers won't merge
            if (flags.has(Options::SCIENTIFIC)) {
                ofs << std::scientific << std::setprecision(PRECISION) << element->evaluate(storage, i);
            } else {
                ofs << std::setprecision(PRECISION) << element->evaluate(storage, i);
            }
        }
        ofs << std::endl;
    }
    ofs.close();
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
    void visit(std::ofstream&, Quantity&) {
        NOT_IMPLEMENTED;
    }
};

template <typename TValue, typename TStoreValue>
static void storeBuffers(Quantity& q, TStoreValue&& storeValue) {
    StaticArray<Array<TValue>&, 3> buffers = q.getAll<TValue>();
    switch (q.getOrderEnum()) {
    case OrderEnum::ZERO:
    case OrderEnum::FIRST:
        for (Size i = 0; i < q.size(); ++i) {
            storeValue(buffers[0][i]);
        }
        break;
    case OrderEnum::SECOND:
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
    ASSERT(!elements.empty() && "nothing to dump");
    const std::string fileName = paths.getNextPath();
    std::ofstream ofs(fileName);
    // file format identifie
    ofs << "SPH" << time << storage.getParticleCnt() << storage.getQuantityCnt();
    // storage dump
    for (auto i : storage.getQuantities()) {
        // first 3 values: quantity ID, order (number of derivatives), type
        Quantity& q = i.quantity;
        ofs << Size(i.id) << Size(q.getOrderEnum()) << Size(q.getValueEnum());
        switch (q.getValueEnum()) {
        case ValueEnum::INDEX:
            storeBuffers<Size>(q, [&ofs](const Size idx) { ofs << idx; });
            break;
        case ValueEnum::SCALAR:
            storeBuffers<Float>(q, [&ofs](const Float f) { ofs << f; });
            break;
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