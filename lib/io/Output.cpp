#include "io/Output.h"
#include "io/Column.h"
#include "io/FileSystem.h"
#include "io/Serializer.h"
#include "quantities/AbstractMaterial.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

OutputFile::OutputFile(const Path& pathMask)
    : pathMask(pathMask) {
    ASSERT(pathMask.native().find("%d", 0) != std::string::npos);
}

Path OutputFile::getNextPath() const {
    std::string path = pathMask.native();
    Size n = path.find("%d", 0);
    std::ostringstream ss;
    ss << std::setw(4) << std::setfill('0') << dumpNum;
    path.replace(n, 2, ss.str());
    dumpNum++;
    return Path(path);
}

Abstract::Output::Output(const Path& fileMask)
    : paths(fileMask) {}


static void printHeader(std::ostream& ofs, const std::string& name, const ValueEnum type) {
    switch (type) {
    case ValueEnum::SCALAR:
    case ValueEnum::INDEX:
        ofs << std::setw(20) << name;
        break;
    case ValueEnum::VECTOR:
        ofs << std::setw(20) << (name + " [x]") << std::setw(20) << (name + " [y]") << std::setw(20)
            << (name + " [z]");
        break;
    case ValueEnum::SYMMETRIC_TENSOR:
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

TextOutput::TextOutput(const Path& fileMask, const std::string& runName, const Flags<Options> flags)
    : Abstract::Output(fileMask)
    , runName(runName)
    , flags(flags) {}

TextOutput::~TextOutput() = default;

Path TextOutput::dump(Storage& storage, const Statistics& stats) {
    ASSERT(!columns.empty(), "No column added to TextOutput");
    const Path fileName = paths.getNextPath();
    createDirectory(fileName.parentPath());
    std::ofstream ofs(fileName.native());
    // print description
    ofs << "# Run: " << runName << std::endl;
    ofs << "# SPH dump, time = " << stats.get<Float>(StatisticsId::TOTAL_TIME) << std::endl;
    ofs << "# ";
    for (auto& column : columns) {
        printHeader(ofs, column->getName(), column->getType());
    }
    ofs << std::endl;
    // print data lines, starting with second-order quantities
    for (Size i = 0; i < storage.getParticleCnt(); ++i) {
        for (auto& column : columns) {
            // write one extra space to be sure numbers won't merge
            if (flags.has(Options::SCIENTIFIC)) {
                ofs << std::scientific << std::setprecision(PRECISION) << column->evaluate(storage, stats, i);
            } else {
                ofs << std::setprecision(PRECISION) << column->evaluate(storage, stats, i);
            }
        }
        ofs << std::endl;
    }
    ofs.close();
    return fileName;
}

Outcome TextOutput::load(const Path& path, Storage& storage) {
    std::ifstream ifs(path.native());
    std::string line;
    storage.removeAll();
    Size i = 0;
    while (std::getline(ifs, line)) {
        if (line[0] == '#') { // comment
            continue;
        }
        std::stringstream ss(line);
        for (auto& column : columns) {
            column->accumulate(storage, 5._f, i);
        }
        i++;
    }
    ifs.close();
    return SUCCESS;
}

void TextOutput::add(AutoPtr<Abstract::Column>&& column) {
    columns.push(std::move(column));
}


Path GnuplotOutput::dump(Storage& storage, const Statistics& stats) {
    const Path path = TextOutput::dump(storage, stats);
    const Path pathWithoutExt = Path(path).removeExtension();
    const Float time = stats.get<Float>(StatisticsId::TOTAL_TIME);
    const std::string command = "gnuplot -e \"filename='" + pathWithoutExt.native() +
                                "'; time=" + std::to_string(time) + "\" " + scriptPath;
    const int returned = system(command.c_str());
    MARK_USED(returned);
    return path;
}

BinaryOutput::BinaryOutput(const Path& fileMask, const std::string& runName)
    : Abstract::Output(fileMask)
    , runName(runName) {}

namespace {
    template <typename TValue, typename TStoreValue>
    void storeBuffers(Quantity& q, const IndexSequence& sequence, TStoreValue&& storeValue) {
        StaticArray<Array<TValue>&, 3> buffers = q.getAll<TValue>();
        for (Size i : sequence) {
            storeValue(buffers[0][i]);
        }
        switch (q.getOrderEnum()) {
        case OrderEnum::ZERO:
            break;
        case OrderEnum::FIRST:
            for (Size i : sequence) {
                storeValue(buffers[1][i]);
            }
            break;
        case OrderEnum::SECOND:
            for (Size i : sequence) {
                storeValue(buffers[1][i]);
            }
            for (Size i : sequence) {
                storeValue(buffers[2][i]);
            }
            break;
        default:
            STOP;
        }
    }
    struct SettingsDispatcher {
        Serializer& serializer;

        template <typename T>
        void operator()(const T& value) {
            serializer.write(value);
        }
        void operator()(const Range& value) {
            serializer.write(value.lower(), value.upper());
        }
        void operator()(const Vector& value) {
            serializer.write(value[X], value[Y], value[Z], value[H]);
        }
        void operator()(const SymmetricTensor& t) {
            serializer.write(t(0, 0), t(1, 1), t(2, 2), t(0, 1), t(0, 2), t(1, 2));
        }
        void operator()(const TracelessTensor& t) {
            serializer.write(t(0, 0), t(1, 1), t(0, 1), t(0, 2), t(1, 2));
        }
    };
}

Path BinaryOutput::dump(Storage& storage, const Statistics& stats) {
    const Path fileName = paths.getNextPath();
    const Float time = stats.get<Float>(StatisticsId::TOTAL_TIME);

    Serializer serializer(fileName);
    // file format identifier
    // size: 4 + 8 + 8 + 8 + 8 = 36
    serializer.write(
        "SPH", time, storage.getParticleCnt(), storage.getQuantityCnt(), storage.getMaterialCnt());
    // zero bytes until 256 to allow extensions of the header
    serializer.addPadding(PADDING_SIZE);


    // quantity information
    Array<QuantityId> cachedIds;
    for (auto i : storage.getQuantities()) {
        // first 3 values: quantity ID, order (number of derivatives), type
        Quantity& q = i.quantity;
        cachedIds.push(i.id);
        serializer.write(Size(i.id), Size(q.getOrderEnum()), Size(q.getValueEnum()));
    }

    SettingsDispatcher dispatcher{ serializer };

    // dump quantities separated by materials
    for (Size matIdx = 0; matIdx < storage.getMaterialCnt(); ++matIdx) {
        serializer.write("MAT", matIdx);
        MaterialView material = storage.getMaterial(matIdx);
        serializer.write(material->getParams().size());
        // dump body settings
        for (auto param : material->getParams()) {
            serializer.write(param.id);
            serializer.write(param.value.getTypeIdx());
            forValue(param.value, dispatcher);
        }
        // dump all ranges and minimal values for timestepping
        serializer.write(cachedIds.size());
        for (QuantityId id : cachedIds) {
            const Range range = material->range(id);
            const Float minimal = material->minimal(id);
            serializer.write(range.lower(), range.upper(), minimal);
        }

        // storage dump for given material
        for (auto i : storage.getQuantities()) {
            Quantity& q = i.quantity;
            serializer.write(*material.sequence().begin(), *material.sequence().end());
            switch (q.getValueEnum()) {
            case ValueEnum::INDEX:
                storeBuffers<Size>(
                    q, material.sequence(), [&serializer](const Size idx) { serializer.write(idx); });
                break;
            case ValueEnum::SCALAR:
                storeBuffers<Float>(
                    q, material.sequence(), [&serializer](const Float f) { serializer.write(f); });
                break;
            case ValueEnum::VECTOR:
                storeBuffers<Vector>(q, material.sequence(), [&serializer](const Vector& v) {
                    // store all components to save smoothing lengths; although it is a waste for velocities
                    // and other vector quantities...
                    serializer.write(v[X], v[Y], v[Z], v[H]);
                });
                break;
            case ValueEnum::TENSOR:
                storeBuffers<Tensor>(q, material.sequence(), [&serializer](const Tensor& t) {
                    // clang-format off
                    serializer.write(t(0, 0), t(0, 1), t(0, 2), //
                                     t(1, 0), t(1, 1), t(1, 2), //
                                     t(2, 0), t(2, 1), t(2, 2));
                    // clang-format on
                });
                break;
            case ValueEnum::SYMMETRIC_TENSOR:
                storeBuffers<SymmetricTensor>(
                    q, material.sequence(), [&serializer](const SymmetricTensor& t) {
                        serializer.write(t(0, 0), t(1, 1), t(2, 2), t(0, 1), t(0, 2), t(1, 2));
                    });
                break;
            case ValueEnum::TRACELESS_TENSOR:
                storeBuffers<TracelessTensor>(
                    q, material.sequence(), [&serializer](const TracelessTensor& t) {
                        serializer.write(t(0, 0), t(1, 1), t(0, 1), t(0, 2), t(1, 2));
                    });
                break;
            default:
                NOT_IMPLEMENTED;
            }
        }
    }


    return fileName;
}

template <typename TValue, typename TLoadValue>
static bool loadBuffers(Storage& storage,
    const IndexSequence& sequence,
    const QuantityId id,
    const OrderEnum order,
    const Size particleCnt,
    const TLoadValue& loadValue) {
    Array<TValue> buffer(particleCnt);
    for (Size i : sequence) {
        if (!loadValue(buffer[i])) {
            return false;
        }
    }
    storage.insert<TValue>(id, order, std::move(buffer));
    switch (order) {
    case OrderEnum::ZERO:
        // already done
        break;
    case OrderEnum::FIRST: {
        ArrayView<TValue> dv = storage.getDt<TValue>(id);
        for (Size i : sequence) {
            if (!loadValue(dv[i])) {
                return false;
            }
        }
        break;
    }
    case OrderEnum::SECOND: {
        ArrayView<TValue> dv = storage.getDt<TValue>(id);
        ArrayView<TValue> d2v = storage.getD2t<TValue>(id);
        for (Size i : sequence) {
            if (!loadValue(dv[i])) {
                return false;
            }
        }
        for (Size i : sequence) {
            if (!loadValue(d2v[i])) {
                return false;
            }
        }
        break;
    }
    default:
        NOT_IMPLEMENTED;
    }
    return true;
}

Outcome BinaryOutput::load(const Path& path, Storage& storage) {
    storage.removeAll();
    Deserializer deserializer(path);
    std::string identifier;
    Float time;
    Size particleCnt, quantityCnt, materialCnt;
    if (!deserializer.read(identifier, time, particleCnt, quantityCnt, materialCnt) || identifier != "SPH") {
        return "Invalid file format";
    }
    if (!deserializer.skip(PADDING_SIZE)) {
        return "Incorrect header size";
    }
    QuantityId id;
    OrderEnum order;
    ValueEnum value;
    if (!deserializer.read(id, order, value)) {
        return "Failed to read quantity information";
    }
    Size loadedQuantities = 0;
    Size matIdx;
    while (deserializer.read(identifier, matIdx)) {

        /// \todo read material
        /// hm this doesnt work, we need the actual material to be able to continue the run!!
        NullMaterial material;
        Storage bodyStorage(material);
        switch (value) {
        case ValueEnum::INDEX:
            if (!loadBuffers<Size>(storage, id, order, particleCnt, [&deserializer](Size& idx) {
                    return deserializer.read(idx);
                })) {
                return makeFailed("Failed reading index quantity ", id);
            };
            break;
        case ValueEnum::SCALAR:
            if (!loadBuffers<Float>(storage, id, order, particleCnt, [&deserializer](Float& f) {
                    return deserializer.read(f);
                })) {
                return makeFailed("Failed reading scalar quantity ", id);
            };
            break;
        case ValueEnum::VECTOR:
            if (!loadBuffers<Vector>(storage, id, order, particleCnt, [&deserializer](Vector& v) {
                    return deserializer.read(v[X], v[Y], v[Z], v[H]);
                })) {
                return makeFailed("Failed reading vector quantity ", id);
            }
            break;
        case ValueEnum::TENSOR:
            if (!loadBuffers<Tensor>(storage, id, order, particleCnt, [&deserializer](Tensor& t) {
                    // clang-format off
                return deserializer.read(t(0, 0), t(0, 1), t(0, 2), //
                                         t(1, 0), t(1, 1), t(1, 2), //
                                         t(2, 0), t(2, 1), t(2, 2));
                    // clang-format on
                })) {
                return makeFailed("Failed reading tensor quantity ", id);
            }
            break;
        case ValueEnum::SYMMETRIC_TENSOR:
            if (!loadBuffers<SymmetricTensor>(
                    storage, id, order, particleCnt, [&deserializer](SymmetricTensor& t) {
                        return deserializer.read(t(0, 0), t(1, 1), t(2, 2), t(0, 1), t(0, 2), t(1, 2));
                    })) {
                return makeFailed("Failed reading symmetric tensor quantity ", id);
            }
            break;
        case ValueEnum::TRACELESS_TENSOR:
            if (!loadBuffers<TracelessTensor>(
                    storage, id, order, particleCnt, [&deserializer](TracelessTensor& t) {
                        StaticArray<Float, 5> a;
                        if (!deserializer.read(a[0], a[1], a[2], a[3], a[4])) {
                            return false;
                        }
                        t = TracelessTensor(a[0], a[1], a[2], a[3], a[4]);
                        return true;
                    })) {
                return makeFailed("Failed reading traceless tensor quantity ", id);
            }
            break;
        default:
            NOT_IMPLEMENTED;
        }

        loadedQuantities++;
    }
    if (loadedQuantities != quantityCnt) {
        return makeFailed(
            "Expected ", quantityCnt, " quantities, but ", loadedQuantities, " quantities has been loaded.");
    }

    return SUCCESS;
}

PkdgravOutput::PkdgravOutput(const Path& fileMask, const std::string& runName, PkdgravParams&& params)
    : Abstract::Output(fileMask)
    , runName(runName)
    , params(std::move(params)) {
    ASSERT(almostEqual(conversion.velocity, 2.97853e6_f));
}

Path PkdgravOutput::dump(Storage& storage, const Statistics& UNUSED(stats)) {
    const Path fileName = paths.getNextPath();

    ArrayView<Float> m, rho, u;
    tie(m, rho, u) = storage.getValues<Float>(QuantityId::MASSES, QuantityId::DENSITY, QuantityId::ENERGY);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    ArrayView<Size> flags = storage.getValue<Size>(QuantityId::FLAG);

    std::ofstream ofs(fileName.native());
    ofs << std::setw(20) << std::setprecision(PRECISION);

    Size idx = 0;
    for (Size i = 0; i < r.size(); ++i) {
        if (u[i] > params.vaporThreshold) {
            continue;
        }
        const Float radius = this->getRadius(r[idx][H], m[idx], rho[idx]);
        ofs << idx << idx << m[idx] / conversion.mass << radius / conversion.distance
            << r[idx] / conversion.distance << v[idx] / conversion.velocity
            << Vector(0._f) /* zero initial rotation */ << params.colors[flags[idx]];
        idx++;
    }
    return fileName;
}

NAMESPACE_SPH_END
