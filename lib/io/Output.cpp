#include "io/Output.h"
#include "io/Column.h"
#include "io/FileSystem.h"
#include "io/Serializer.h"
#include "objects/finders/Order.h"
#include "quantities/IMaterial.h"
#include "system/Factory.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

/// ----------------------------------------------------------------------------------------------------------
/// OutputFile
/// ----------------------------------------------------------------------------------------------------------

OutputFile::OutputFile(const Path& pathMask, const Size firstDumpIdx)
    : pathMask(pathMask) {
    dumpNum = firstDumpIdx;
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
        const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
        ss << std::fixed << t;
        /// \todo replace decimal dot as docs say
        path.replace(n, 2, ss.str());
    }
    dumpNum++;
    return Path(path);
}

bool OutputFile::hasWildcard() const {
    std::string path = pathMask.native();
    return path.find("%d") != std::string::npos || path.find("%t") != std::string::npos;
}

Path OutputFile::getMask() const {
    return pathMask;
}

IOutput::IOutput(const OutputFile& fileMask)
    : paths(fileMask) {
    ASSERT(!fileMask.getMask().empty());
}

/// ----------------------------------------------------------------------------------------------------------
/// TextOutput/Input
/// ----------------------------------------------------------------------------------------------------------

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

static void addColumns(const Flags<OutputQuantityFlag> quantities, Array<AutoPtr<ITextColumn>>& columns) {
    if (quantities.has(OutputQuantityFlag::POSITION)) {
        columns.push(makeAuto<ValueColumn<Vector>>(QuantityId::POSITION));
    }
    if (quantities.has(OutputQuantityFlag::VELOCITY)) {
        columns.push(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITION));
    }
    if (quantities.has(OutputQuantityFlag::SMOOTHING_LENGTH)) {
        columns.push(makeAuto<SmoothingLengthColumn>());
    }
    if (quantities.has(OutputQuantityFlag::MASS)) {
        columns.push(makeAuto<ValueColumn<Float>>(QuantityId::MASS));
    }
    if (quantities.has(OutputQuantityFlag::PRESSURE)) {
        columns.push(makeAuto<ValueColumn<Float>>(QuantityId::PRESSURE));
    }
    if (quantities.has(OutputQuantityFlag::DENSITY)) {
        columns.push(makeAuto<ValueColumn<Float>>(QuantityId::DENSITY));
    }
    if (quantities.has(OutputQuantityFlag::ENERGY)) {
        columns.push(makeAuto<ValueColumn<Float>>(QuantityId::ENERGY));
    }
    if (quantities.has(OutputQuantityFlag::DEVIATORIC_STRESS)) {
        columns.push(makeAuto<ValueColumn<TracelessTensor>>(QuantityId::DEVIATORIC_STRESS));
    }
    if (quantities.has(OutputQuantityFlag::DAMAGE)) {
        columns.push(makeAuto<ValueColumn<Float>>(QuantityId::DAMAGE));
    }
    if (quantities.has(OutputQuantityFlag::STRAIN_RATE_CORRECTION_TENSOR)) {
        columns.push(makeAuto<ValueColumn<SymmetricTensor>>(QuantityId::STRAIN_RATE_CORRECTION_TENSOR));
    }
    if (quantities.has(OutputQuantityFlag::MATERIAL_ID)) {
        columns.push(makeAuto<ValueColumn<Size>>(QuantityId::MATERIAL_ID));
    }
    if (quantities.has(OutputQuantityFlag::INDEX)) {
        columns.push(makeAuto<ParticleNumberColumn>());
    }
}

struct DumpAllVisitor {
    template <typename TValue>
    void visit(QuantityId id, Array<AutoPtr<ITextColumn>>& columns) {
        columns.push(makeAuto<ValueColumn<TValue>>(id));
    }
};

TextOutput::TextOutput(const OutputFile& fileMask,
    const std::string& runName,
    const Flags<OutputQuantityFlag> quantities,
    const Flags<Options> options)
    : IOutput(fileMask)
    , runName(runName)
    , options(options) {
    addColumns(quantities, columns);
}

TextOutput::~TextOutput() = default;

Path TextOutput::dump(Storage& storage, const Statistics& stats) {
    if (options.has(Options::DUMP_ALL)) {
        columns.clear();
        // add some 'extraordinary' quantities and position (we want those to be one of the first, not after
        // density, etc).
        columns.push(makeAuto<ParticleNumberColumn>());
        columns.push(makeAuto<ValueColumn<Vector>>(QuantityId::POSITION));
        columns.push(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITION));
        columns.push(makeAuto<SmoothingLengthColumn>());
        for (StorageElement e : storage.getQuantities()) {
            if (e.id == QuantityId::POSITION) {
                // already added
                continue;
            }
            dispatch(e.quantity.getValueEnum(), DumpAllVisitor{}, e.id, columns);
        }
    }

    ASSERT(!columns.empty(), "No column added to TextOutput");
    const Path fileName = paths.getNextPath(stats);
    FileSystem::createDirectory(fileName.parentPath());
    std::ofstream ofs(fileName.native());
    // print description
    ofs << "# Run: " << runName << std::endl;
    ofs << "# SPH dump, time = " << stats.get<Float>(StatisticsId::RUN_TIME) << std::endl;
    ofs << "# ";
    for (auto& column : columns) {
        printHeader(ofs, column->getName(), column->getType());
    }
    ofs << std::endl;
    // print data lines, starting with second-order quantities
    for (Size i = 0; i < storage.getParticleCnt(); ++i) {
        for (auto& column : columns) {
            // write one extra space to be sure numbers won't merge
            if (options.has(Options::SCIENTIFIC)) {
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

TextOutput& TextOutput::addColumn(AutoPtr<ITextColumn>&& column) {
    columns.push(std::move(column));
    return *this;
}

TextInput::TextInput(Flags<OutputQuantityFlag> quantities) {
    addColumns(quantities, columns);
}

Outcome TextInput::load(const Path& path, Storage& storage, Statistics& UNUSED(stats)) {
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
                case ValueEnum::TRACELESS_TENSOR: {
                    Float xx, yy, xy, xz, yz;
                    ss >> xx >> yy >> xy >> xz >> yz;
                    TracelessTensor t(xx, yy, xy, xz, yz);
                    column->accumulate(storage, t, particleCnt);
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

        // sanity check
        if (storage.getParticleCnt() != particleCnt || !storage.isValid()) {
            return "Loaded storage is not valid";
        }

    } catch (std::exception& e) {
        return e.what();
    }
    return SUCCESS;
}

TextInput& TextInput::addColumn(AutoPtr<ITextColumn>&& column) {
    columns.push(std::move(column));
    return *this;
}

/// ----------------------------------------------------------------------------------------------------------
/// GnuplotOutput
/// ----------------------------------------------------------------------------------------------------------

Path GnuplotOutput::dump(Storage& storage, const Statistics& stats) {
    const Path path = TextOutput::dump(storage, stats);
    const Path pathWithoutExt = Path(path).removeExtension();
    const Float time = stats.get<Float>(StatisticsId::RUN_TIME);
    const std::string command = "gnuplot -e \"filename='" + pathWithoutExt.native() +
                                "'; time=" + std::to_string(time) + "\" " + scriptPath;
    const int returned = system(command.c_str());
    MARK_USED(returned);
    return path;
}

/// ----------------------------------------------------------------------------------------------------------
/// BinaryOutput/Input
/// ----------------------------------------------------------------------------------------------------------

namespace {

/// \todo this should be really part of the serializer/deserializer, otherwise it's kinda pointless
struct SerializerDispatcher {
    Serializer& serializer;

    template <typename T>
    void operator()(const T& value) {
        serializer.write(value);
    }
    void operator()(const Interval& value) {
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
    void operator()(const EnumWrapper& e) {
        // Type hash can be different between invocations, so we cannot serialize it. Write 0 (for backward
        // compatibility)
        serializer.write(e.value, 0);
    }
    void operator()(const ClonePtr<ISettingsValue>&) {
        NOT_IMPLEMENTED;
    }
};
struct DeserializerDispatcher {
    Deserializer& deserializer;

    template <typename T>
    void operator()(T& value) {
        deserializer.read(value);
    }
    void operator()(Interval& value) {
        Float lower, upper;
        deserializer.read(lower, upper);
        value = Interval(lower, upper);
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
        deserializer.read(t(0, 0), t(0, 1), t(0, 2), t(1, 0), t(1, 1), t(1, 2), t(2, 0), t(2, 1), t(2, 2));
    }
    void operator()(EnumWrapper& e) {
        deserializer.read(e.value, e.typeHash);
    }
    void operator()(ClonePtr<ISettingsValue>&) {
        NOT_IMPLEMENTED;
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
} // namespace

BinaryOutput::BinaryOutput(const OutputFile& fileMask, const RunTypeEnum runTypeId)
    : IOutput(fileMask)
    , runTypeId(runTypeId) {}

Path BinaryOutput::dump(Storage& storage, const Statistics& stats) {
    const Path fileName = paths.getNextPath(stats);
    const Float time = stats.get<Float>(StatisticsId::RUN_TIME);

    Serializer serializer(fileName);
    // file format identifier
    const Size materialCnt = storage.getMaterialCnt();
    const Size quantityCnt = storage.getQuantityCnt() - int(storage.has(QuantityId::MATERIAL_ID));
    const Float timeStep = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
    serializer.write(
        "SPH", time, storage.getParticleCnt(), quantityCnt, materialCnt, timeStep, BinaryIoVersion::LATEST);
    // write run type
    char runTypeBuffer[16];
    const std::string runTypeStr = EnumMap::toString(runTypeId);
    for (Size i = 0; i < 16; ++i) {
        if (i < runTypeStr.size()) {
            runTypeBuffer[i] = runTypeStr[i];
        } else {
            runTypeBuffer[i] = '\0';
        }
    }
    serializer.write(runTypeBuffer);

    // zero bytes until 256 to allow extensions of the header
    serializer.addPadding(PADDING_SIZE);

    // quantity information
    Array<QuantityId> cachedIds;
    for (auto i : storage.getQuantities()) {
        // first 3 values: quantity ID, order (number of derivatives), type
        Quantity& q = i.quantity;
        if (i.id != QuantityId::MATERIAL_ID) {
            // no need to dump material IDs, they are always consecutive
            cachedIds.push(i.id);
            serializer.write(Size(i.id), Size(q.getOrderEnum()), Size(q.getValueEnum()));
        }
    }

    SerializerDispatcher dispatcher{ serializer };
    const bool hasMaterials = materialCnt > 0;
    // dump quantities separated by materials
    for (Size matIdx = 0; matIdx < max(materialCnt, Size(1)); ++matIdx) {
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
                const Interval range = material->range(id);
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
            if (i.id != QuantityId::MATERIAL_ID) {
                Quantity& q = i.quantity;
                StoreBuffersVisitor visitor;
                dispatch(q.getValueEnum(), visitor, q, serializer, sequence);
            }
        }
    }

    return fileName;
}

template <typename T>
static void setTypeHash(const BodySettings& UNUSED(settings),
    const BodySettingsId UNUSED(paramId),
    T& UNUSED(entry)) {
    // do nothing for other types
}

template <>
void setTypeHash(const BodySettings& settings, const BodySettingsId paramId, EnumWrapper& entry) {
    EnumWrapper current = settings.get<EnumWrapper>(paramId);
    entry.typeHash = current.typeHash;
}


static Expected<Storage> loadMaterial(const Size matIdx,
    Deserializer& deserializer,
    ArrayView<QuantityId> ids,
    const BinaryIoVersion version) {
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
    BodySettings body;
    for (Size i = 0; i < matParamCnt; ++i) {
        // read body settings
        BodySettingsId paramId;
        Size valueId;
        deserializer.read(paramId, valueId);

        if (version == BinaryIoVersion::FIRST) {
            if (valueId == 1 && body.has<EnumWrapper>(paramId)) {
                // enums used to be stored as ints (index 1), now we store it as enum wrapper;
                // convert the value to enum and save manually
                EnumWrapper e = body.get<EnumWrapper>(paramId);
                deserializer.read(e.value);
                body.set(paramId, e);
                continue;
            }
        }

        /// \todo this is currently the only way to access Settings variant, refactor if possible
        SettingsIterator<BodySettingsId>::IteratorValue iteratorValue{ paramId,
            { CONSTRUCT_TYPE_IDX, valueId } };

        forValue(iteratorValue.value, [&deserializer, &body, paramId](auto& entry) {
            DeserializerDispatcher{ deserializer }(entry);
            // little hack: EnumWrapper is loaded with typeHash 0 (as it cannot be serialized, so we have to
            // set it to correct value, otherwise it would trigger asserts in set function.
            setTypeHash(body, paramId, entry);
            body.set(paramId, entry);
        });
    }

    // create material based on settings
    AutoPtr<IMaterial> material = Factory::getMaterial(body);
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
        Interval range;
        if (lower < upper) {
            range = Interval(lower, upper);
        } else {
            range = Interval::unbounded();
        }
        material->setRange(id, range, minimal);
    }
    // create storage for this material
    return Storage(std::move(material));
}

static Optional<RunTypeEnum> readRunType(char* buffer, const BinaryIoVersion UNUSED_IN_RELEASE(version)) {
    std::string runTypeStr(buffer);
    if (!runTypeStr.empty()) {
        return EnumMap::fromString<RunTypeEnum>(runTypeStr).value();
    } else {
        ASSERT(version < BinaryIoVersion::V2018_10_24);
        return NOTHING;
    }
}

Outcome BinaryInput::load(const Path& path, Storage& storage, Statistics& stats) {
    storage.removeAll();
    Deserializer deserializer(path);
    std::string identifier;
    Float time, timeStep;
    Size particleCnt, quantityCnt, materialCnt;
    BinaryIoVersion version;
    char runTypeBuffer[16];
    try {
        deserializer.read(
            identifier, time, particleCnt, quantityCnt, materialCnt, timeStep, version, runTypeBuffer);
    } catch (SerializerException&) {
        return "Invalid file format";
    }
    if (identifier != "SPH") {
        return "Invalid format specifier: expected SPH, got " + identifier;
    }
    stats.set(StatisticsId::RUN_TIME, time);
    stats.set(StatisticsId::TIMESTEP_VALUE, timeStep);
    try {
        deserializer.skip(BinaryOutput::PADDING_SIZE);
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
                Expected<Storage> loadedStorage = loadMaterial(matIdx, deserializer, quantityIds, version);
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

Expected<BinaryInput::Info> BinaryInput::getInfo(const Path& path) const {
    Deserializer deserializer(path);
    std::string identifier;
    Info info;
    char runTypeBuffer[16];
    try {
        deserializer.read(identifier,
            info.runTime,
            info.particleCnt,
            info.quantityCnt,
            info.materialCnt,
            info.timeStep,
            info.version,
            runTypeBuffer);
    } catch (SerializerException&) {
        return makeUnexpected<Info>("Invalid file format");
    }
    if (identifier != "SPH") {
        return makeUnexpected<Info>("Invalid format specifier: expected SPH, got " + identifier);
    }
    info.runType = readRunType(runTypeBuffer, info.version);
    return std::move(info);
}

/// ----------------------------------------------------------------------------------------------------------
/// PkdgravOutput/Input
/// ----------------------------------------------------------------------------------------------------------

PkdgravOutput::PkdgravOutput(const OutputFile& fileMask, PkdgravParams&& params)
    : IOutput(fileMask)
    , params(std::move(params)) {
    ASSERT(almostEqual(this->params.conversion.velocity, 2.97853e4_f, 1.e-4_f));
}

Path PkdgravOutput::dump(Storage& storage, const Statistics& stats) {
    const Path fileName = paths.getNextPath(stats);
    FileSystem::createDirectory(fileName.parentPath());

    ArrayView<Float> m, rho, u;
    tie(m, rho, u) = storage.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY, QuantityId::ENERGY);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
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

Outcome PkdgravInput::load(const Path& path, Storage& storage, Statistics& stats) {
    TextInput input(EMPTY_FLAGS);

    // 1) Particle index -- we don't really need that, just add dummy columnm
    class DummyColumn : public ITextColumn {
    private:
        ValueEnum type;

    public:
        DummyColumn(const ValueEnum type)
            : type(type) {}

        virtual Dynamic evaluate(const Storage&, const Statistics&, const Size) const override {
            NOT_IMPLEMENTED;
        }

        virtual void accumulate(Storage&, const Dynamic, const Size) const override {}

        virtual std::string getName() const override {
            return "dummy";
        }

        virtual ValueEnum getType() const override {
            return type;
        }
    };
    input.addColumn(makeAuto<DummyColumn>(ValueEnum::INDEX));

    // 2) Original index -- not really needed, skip
    input.addColumn(makeAuto<DummyColumn>(ValueEnum::INDEX));

    // 3) Particle mass
    input.addColumn(makeAuto<ValueColumn<Float>>(QuantityId::MASS));

    // 4) radius ?  -- skip
    input.addColumn(makeAuto<ValueColumn<Float>>(QuantityId::DENSITY));

    // 5) Positions (3 components)
    input.addColumn(makeAuto<ValueColumn<Vector>>(QuantityId::POSITION));

    // 6) Velocities (3 components)
    input.addColumn(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITION));

    // 7) Angular velocities (3 components)
    input.addColumn(makeAuto<ValueColumn<Vector>>(QuantityId::ANGULAR_FREQUENCY));

    // 8) Color index -- skip
    input.addColumn(makeAuto<DummyColumn>(ValueEnum::INDEX));

    Outcome outcome = input.load(path, storage, stats);

    if (!outcome) {
        return outcome;
    }

    // whole code assumes positions is a 2nd order quantity, so we have to add the acceleration
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, Vector(0._f));

    // Convert units -- assuming default conversion values
    PkdgravParams::Conversion conversion;
    Array<Vector>& r = storage.getValue<Vector>(QuantityId::POSITION);
    Array<Vector>& v = storage.getDt<Vector>(QuantityId::POSITION);
    Array<Float>& m = storage.getValue<Float>(QuantityId::MASS);
    Array<Float>& rho = storage.getValue<Float>(QuantityId::DENSITY);
    Array<Vector>& omega = storage.getValue<Vector>(QuantityId::ANGULAR_FREQUENCY);

    for (Size i = 0; i < r.size(); ++i) {
        r[i] *= conversion.distance;
        v[i] *= conversion.velocity;
        m[i] *= conversion.mass;

        // compute radius, using the density formula
        /// \todo here we actually store radius in rho ...
        rho[i] *= conversion.distance;
        r[i][H] = root<3>(3.f * m[i] / (2700.f * 4.f * PI));

        // replace the radius with actual density
        /// \todo too high, fix
        rho[i] = m[i] / pow<3>(rho[i]);

        omega[i] *= conversion.velocity / conversion.distance;
    }

    // sort
    Order order(r.size());
    order.shuffle([&m](const Size i1, const Size i2) { return m[i1] > m[i2]; });
    r = order.apply(r);
    v = order.apply(v);
    m = order.apply(m);
    rho = order.apply(rho);
    omega = order.apply(omega);

    return SUCCESS;
}

NAMESPACE_SPH_END
