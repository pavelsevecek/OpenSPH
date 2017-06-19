#include "io/Output.h"
#include "io/Column.h"
#include "io/FileSystem.h"
#include "io/Serializer.h"
#include "quantities/AbstractMaterial.h"
#include "system/Factory.h"
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

    /// \todo this should be really part of the serializer/deserializer, otherwise it's kinda pointless
    struct SerializerDispatcher {
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
        void operator()(const Tensor& t) {
            serializer.write(t(0, 0), t(0, 1), t(0, 2), t(1, 0), t(1, 1), t(1, 2), t(2, 0), t(2, 1), t(2, 2));
        }
    };
    struct DeserializerDispatcher {
        Deserializer& deserializer;

        template <typename T>
        void operator()(T& value) {
            deserializer.read(value);
        }
        void operator()(Range& value) {
            Float lower, upper;
            deserializer.read(lower, upper);
            value = Range(lower, upper);
        }
        void operator()(Vector& value) {
            deserializer.read(value[X], value[Y], value[Z], value[H]);
        }
        void operator()(SymmetricTensor& t) {
            deserializer.read(t(0, 0), t(1, 1), t(2, 2), t(0, 1), t(0, 2), t(1, 2));
        }
        void operator()(TracelessTensor& t) {
            StaticArray<Float, 5> a;
            deserializer.read(a[0], a[1], a[2], a[3], a[4]);
            t = TracelessTensor(a[0], a[1], a[2], a[3], a[4]);
        }
        void operator()(Tensor& t) {
            deserializer.read(
                t(0, 0), t(0, 1), t(0, 2), t(1, 0), t(1, 1), t(1, 2), t(2, 0), t(2, 1), t(2, 2));
        }
    };

    struct StoreBuffersVisitor {
        template <typename TValue>
        void visit(Quantity& q, Serializer& serializer, const IndexSequence& sequence) {
            SerializerDispatcher dispatcher{ serializer };
            StaticArray<Array<TValue>&, 3> buffers = q.getAll<TValue>();
            for (Size i : sequence) {
                dispatcher(buffers[0][i]);
            }
            switch (q.getOrderEnum()) {
            case OrderEnum::ZERO:
                break;
            case OrderEnum::FIRST:
                for (Size i : sequence) {
                    dispatcher(buffers[1][i]);
                }
                break;
            case OrderEnum::SECOND:
                for (Size i : sequence) {
                    dispatcher(buffers[1][i]);
                }
                for (Size i : sequence) {
                    dispatcher(buffers[2][i]);
                }
                break;
            default:
                STOP;
            }
        }
    };

    struct LoadBuffersVisitor {
        template <typename TValue>
        void visit(Storage& storage,
            Deserializer& deserializer,
            const IndexSequence& sequence,
            const QuantityId id,
            const OrderEnum order) {
            DeserializerDispatcher dispatcher{ deserializer };
            Array<TValue> buffer(sequence.size());
            for (Size i : sequence) {
                dispatcher(buffer[i]);
            }
            storage.insert<TValue>(id, order, std::move(buffer));
            switch (order) {
            case OrderEnum::ZERO:
                // already done
                break;
            case OrderEnum::FIRST: {
                ArrayView<TValue> dv = storage.getDt<TValue>(id);
                for (Size i : sequence) {
                    dispatcher(dv[i]);
                }
                break;
            }
            case OrderEnum::SECOND: {
                ArrayView<TValue> dv = storage.getDt<TValue>(id);
                ArrayView<TValue> d2v = storage.getD2t<TValue>(id);
                for (Size i : sequence) {
                    dispatcher(dv[i]);
                }
                for (Size i : sequence) {
                    dispatcher(d2v[i]);
                }
                break;
            }
            default:
                NOT_IMPLEMENTED;
            }
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

    SerializerDispatcher dispatcher{ serializer };
    const bool hasMaterials = storage.getMaterialCnt() > 0;
    // dump quantities separated by materials
    for (Size matIdx = 0; matIdx < max(storage.getMaterialCnt(), Size(1)); ++matIdx) {
        // storage can currently exist without materials, only write material params if we have a material
        if (hasMaterials) {
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
            for (QuantityId id : cachedIds) {
                const Range range = material->range(id);
                const Float minimal = material->minimal(id);
                serializer.write(id, range.lower(), range.upper(), minimal);
            }
        } else {
            // write that we have no materials
            serializer.write("NOMAT");
        }

        // storage dump for given material
        IndexSequence sequence = [&] {
            if (hasMaterials) {
                MaterialView material = storage.getMaterial(matIdx);
                return material.sequence();
            } else {
                return IndexSequence(0, storage.getParticleCnt());
            }
        }();
        serializer.write(*sequence.begin(), *sequence.end());

        for (auto i : storage.getQuantities()) {
            Quantity& q = i.quantity;
            StoreBuffersVisitor visitor;
            dispatch(q.getValueEnum(), visitor, q, serializer, sequence);
        }
    }

    return fileName;
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

    struct QuantityInfo {
        QuantityId id;
        OrderEnum order;
        ValueEnum value;
    };
    Array<QuantityInfo> infos(quantityCnt);
    for (QuantityInfo& i : infos) {
        deserializer.read(i.id, i.order, i.value);
    }

    // Size loadedQuantities = 0;
    const bool hasMaterials = materialCnt > 0;
    for (Size matIdx = 0; matIdx < max(materialCnt, Size(1)); ++matIdx) {
        Storage bodyStorage;
        if (hasMaterials) {
            Size matIdxCheck;
            deserializer.read(identifier, matIdxCheck);
            ASSERT(identifier == "MAT");
            ASSERT(matIdxCheck == matIdx);
            Size matParamCnt;
            deserializer.read(matParamCnt);
            BodySettings settings;
            for (Size i = 0; i < matParamCnt; ++i) {
                // read body settings
                BodySettingsId paramId;
                Size valueId;
                /// \todo would be much easier if the deserializer just threw an exception instead of checking
                /// every single read
                deserializer.read(paramId, valueId);
                /// \todo this is currently the only way to access Settings variant, refactor if possible
                SettingsIterator<BodySettingsId>::IteratorValue iteratorValue{ paramId,
                    { CONSTRUCT_TYPE_IDX, valueId } };
                forValue(iteratorValue.value, [&deserializer, &settings, paramId](auto& entry) {
                    DeserializerDispatcher{ deserializer }(entry);
                    settings.set(paramId, entry);
                });
            }

            // create material based on settings
            AutoPtr<Abstract::Material> material = Factory::getMaterial(settings);
            // read all ranges and minimal values for timestepping
            for (Size i = 0; i < quantityCnt; ++i) {
                QuantityId id;
                Float lower, upper, minimal;
                deserializer.read(id, lower, upper, minimal);
                /// \todo replace asserts with exceptions
                ASSERT(id == infos[i].id);
                if (lower < upper) {
                    material->range(id) = Range(lower, upper);
                }
                material->minimal(id) = minimal;
            }
            // create storage for this material
            bodyStorage = Storage(std::move(material));
        } else {
            deserializer.read(identifier);
            ASSERT(identifier == "NOMAT");
        }

        Size from, to;
        deserializer.read(from, to);
        LoadBuffersVisitor visitor;
        for (Size i = 0; i < quantityCnt; ++i) {
            dispatch(infos[i].value,
                visitor,
                bodyStorage,
                deserializer,
                IndexSequence(0, to - from),
                infos[i].id,
                infos[i].order);
        }
        storage.merge(std::move(bodyStorage));
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
