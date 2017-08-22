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
    ASSERT(!pathMask.empty());
}

Path OutputFile::getNextPath(const Statistics& stats) const {
    ASSERT(!pathMask.empty());
    std::string path = pathMask.native();
    std::size_t n = path.find("%d");
    if (n != std::string::npos) {
        std::ostringstream ss;
        ss << std::setw(4) << std::setfill('0') << dumpNum;
        path.replace(n, 2, ss.str());
    }
    n = path.find("%t");
    if (n != std::string::npos) {
        std::ostringstream ss;
        const Float t = stats.get<Float>(StatisticsId::TOTAL_TIME);
        ss << std::fixed << t;
        /// \todo replace decimal dot as docs say
        path.replace(n, 2, ss.str());
    }
    dumpNum++;
    return Path(path);
}

Abstract::Output::Output(const Path& fileMask)
    : paths(fileMask) {
    ASSERT(!fileMask.empty());
}

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
    const Path fileName = paths.getNextPath(stats);
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
    try {
        std::ifstream ifs(path.native());
        if (!ifs) {
            return "Failed to open the file";
        }
        std::string line;
        storage.removeAll();
        // storage currently requires at least one quantity for insertion by value
        Quantity& flags = storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Array<Size>{ 0 });

        Size particleCnt = 0;
        while (std::getline(ifs, line)) {
            if (line[0] == '#') { // comment
                continue;
            }
            std::stringstream ss(line);
            for (auto& column : columns) {
                switch (column->getType()) {
                /// \todo de-duplicate the loading (used in Settings)
                case ValueEnum::INDEX: {
                    Size i;
                    ss >> i;
                    column->accumulate(storage, i, particleCnt);
                    break;
                }
                case ValueEnum::SCALAR: {
                    Float f;
                    ss >> f;
                    column->accumulate(storage, f, particleCnt);
                    break;
                }
                case ValueEnum::VECTOR: {
                    Vector v(0._f);
                    ss >> v[X] >> v[Y] >> v[Z];
                    column->accumulate(storage, v, particleCnt);
                    break;
                }
                default:
                    NOT_IMPLEMENTED;
                }
            }
            particleCnt++;
        }
        ifs.close();

        // resize the flag quantity to make the storage consistent
        for (Array<Size>& buffer : flags.getAll<Size>()) {
            buffer.resize(particleCnt);
        }

        // sanity chek
        if (storage.getParticleCnt() != particleCnt || !storage.isValid()) {
            return "Loaded storage is not valid";
        }

    } catch (std::exception& e) {
        return e.what();
    }
    return SUCCESS;
}

TextOutput& TextOutput::add(AutoPtr<Abstract::Column>&& column) {
    columns.push(std::move(column));
    return *this;
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

BinaryOutput::BinaryOutput(const Path& fileMask)
    : Abstract::Output(fileMask) {}

Path BinaryOutput::dump(Storage& storage, const Statistics& stats) {
    const Path fileName = paths.getNextPath(stats);
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


static Expected<Storage> loadMaterial(const Size matIdx,
    Deserializer& deserializer,
    ArrayView<QuantityId> ids) {
    Size matIdxCheck;
    std::string identifier;
    deserializer.read(identifier, matIdxCheck);
    // some consistency checks
    if (identifier != "MAT") {
        return makeUnexpected<Storage>("Invalid material identifier, expected MAT, got " + identifier);
    }
    if (matIdxCheck != matIdx) {
        return makeUnexpected<Storage>("Unexpected material index, expected " + std::to_string(matIdx) +
                                       ", got " + std::to_string(matIdxCheck));
    }

    Size matParamCnt;
    deserializer.read(matParamCnt);
    BodySettings settings;
    for (Size i = 0; i < matParamCnt; ++i) {
        // read body settings
        BodySettingsId paramId;
        Size valueId;
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
    for (Size i = 0; i < ids.size(); ++i) {
        QuantityId id;
        Float lower, upper, minimal;
        deserializer.read(id, lower, upper, minimal);
        if (id != ids[i]) {
            return makeUnexpected<Storage>("Unexpected quantityId, expected " +
                                           getMetadata(ids[i]).quantityName + ", got " +
                                           getMetadata(id).quantityName);
        }
        if (lower < upper) {
            material->range(id) = Range(lower, upper);
        }
        material->minimal(id) = minimal;
    }
    // create storage for this material
    return Storage(std::move(material));
}


Outcome BinaryOutput::load(const Path& path, Storage& storage) {
    storage.removeAll();
    Deserializer deserializer(path);
    std::string identifier;
    Float time;
    Size particleCnt, quantityCnt, materialCnt;
    try {
        deserializer.read(identifier, time, particleCnt, quantityCnt, materialCnt);
    } catch (SerializerException&) {
        return "Invalid file format";
    }
    if (identifier != "SPH") {
        return "Invalid format specifier: expected SPH, got " + identifier;
    }
    try {
        deserializer.skip(PADDING_SIZE);
    } catch (SerializerException&) {
        return "Incorrect header size";
    }

    Array<QuantityId> quantityIds(quantityCnt);
    Array<OrderEnum> orders(quantityCnt);
    Array<ValueEnum> valueTypes(quantityCnt);
    try {
        for (Size i = 0; i < quantityCnt; ++i) {
            deserializer.read(quantityIds[i], orders[i], valueTypes[i]);
        }
    } catch (SerializerException& e) {
        return e.what();
    }

    // Size loadedQuantities = 0;
    const bool hasMaterials = materialCnt > 0;
    for (Size matIdx = 0; matIdx < max(materialCnt, Size(1)); ++matIdx) {
        Storage bodyStorage;
        if (hasMaterials) {
            try {
                Expected<Storage> loadedStorage = loadMaterial(matIdx, deserializer, quantityIds);
                if (!loadedStorage) {
                    return loadedStorage.error();
                } else {
                    bodyStorage = std::move(loadedStorage.value());
                }
            } catch (SerializerException& e) {
                return e.what();
            }
        } else {
            try {
                deserializer.read(identifier);
            } catch (SerializerException& e) {
                return e.what();
            }
            if (identifier != "NOMAT") {
                return "Unexpected missing material identifier, expected NOMAT, got " + identifier;
            }
        }

        try {
            Size from, to;
            deserializer.read(from, to);
            LoadBuffersVisitor visitor;
            for (Size i = 0; i < quantityCnt; ++i) {
                dispatch(valueTypes[i],
                    visitor,
                    bodyStorage,
                    deserializer,
                    IndexSequence(0, to - from),
                    quantityIds[i],
                    orders[i]);
            }
        } catch (SerializerException& e) {
            return e.what();
        }
        storage.merge(std::move(bodyStorage));
    }

    return SUCCESS;
}


PkdgravOutput::PkdgravOutput(const Path& fileMask, PkdgravParams&& params)
    : Abstract::Output(fileMask)
    , params(std::move(params)) {
    ASSERT(almostEqual(this->params.conversion.velocity, 2.97853e4_f, 1.e-4_f));
}

Path PkdgravOutput::dump(Storage& storage, const Statistics& stats) {
    const Path fileName = paths.getNextPath(stats);

    ArrayView<Float> m, rho, u;
    tie(m, rho, u) = storage.getValues<Float>(QuantityId::MASSES, QuantityId::DENSITY, QuantityId::ENERGY);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    ArrayView<Size> flags = storage.getValue<Size>(QuantityId::FLAG);

    std::ofstream ofs(fileName.native());
    ofs << std::setprecision(PRECISION) << std::scientific;

    Size idx = 0;
    for (Size i = 0; i < r.size(); ++i) {
        if (u[i] > params.vaporThreshold) {
            continue;
        }
        const Float radius = this->getRadius(r[idx][H], m[idx], rho[idx]);
        const Vector v_in = v[idx] + cross(params.omega, r[idx]);
        ASSERT(flags[idx] < params.colors.size(), flags[idx], params.colors.size());
        ofs << std::setw(25) << idx <<                                   //
            std::setw(25) << idx <<                                      //
            std::setw(25) << m[idx] / params.conversion.mass <<          //
            std::setw(25) << radius / params.conversion.distance <<      //
            std::setw(25) << r[idx] / params.conversion.distance <<      //
            std::setw(25) << v_in / params.conversion.velocity <<        //
            std::setw(25) << Vector(0._f) /* zero initial rotation */ << //
            std::setw(25) << params.colors[flags[idx]] << std::endl;
        idx++;
    }
    return fileName;
}

NAMESPACE_SPH_END
