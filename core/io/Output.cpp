#include "io/Output.h"
#include "io/Column.h"
#include "io/FileSystem.h"
#include "io/Logger.h"
#include "io/Serializer.h"
#include "objects/finders/Order.h"
#include "post/TwoBody.h"
#include "quantities/Attractor.h"
#include "quantities/IMaterial.h"
#include "system/Factory.h"
#include <fstream>

#ifdef SPH_USE_HDF5
#include <hdf5.h>
#endif

NAMESPACE_SPH_BEGIN

// ----------------------------------------------------------------------------------------------------------
// OutputFile
// ----------------------------------------------------------------------------------------------------------

OutputFile::OutputFile(const Path& pathMask, const Size firstDumpIdx)
    : pathMask(pathMask) {
    dumpNum = firstDumpIdx;
    SPH_ASSERT(!pathMask.empty());
}

Path OutputFile::getNextPath(const Statistics& stats) const {
    SPH_ASSERT(!pathMask.empty());
    String path = pathMask.string();
    Size n = path.find(L"%d");
    if (n != String::npos) {
        std::wostringstream ss;
        ss << std::setw(4) << std::setfill(L'0') << dumpNum;
        path.replace(n, 2, String::fromWstring(ss.str()));
    }
    n = path.find(L"%t");
    if (n != String::npos) {
        std::wostringstream ss;
        const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
        ss << std::fixed << t;
        /// \todo replace decimal dot as docs say
        path.replace(n, 2, String::fromWstring(ss.str()));
    }
    dumpNum++;
    return Path(path);
}

Optional<Size> OutputFile::getDumpIdx(const Path& path) {
    // look for 4 consecutive digits.
    const String s = path.fileName().string();
    for (int i = 0; i < int(s.size()) - 3; ++i) {
        if (std::isdigit(s[i]) && std::isdigit(s[i + 1]) && std::isdigit(s[i + 2]) &&
            std::isdigit(s[i + 3])) {
            // next digit must NOT be a number
            if (i + 4 < int(s.size()) && std::isdigit(s[i + 4])) {
                // 4-digit sequence is not unique, report error
                return NOTHING;
            }
            Optional<Size> index = fromString<Size>(s.substr(i, 4));
            SPH_ASSERT(index);
            if (index) {
                return index.value();
            } else {
                return NOTHING;
            }
        }
    }
    return NOTHING;
}

Optional<OutputFile> OutputFile::getMaskFromPath(const Path& path, const Size firstDumpIdx) {
    /// \todo could be deduplicated a bit
    const String s = path.fileName().string();
    for (int i = 0; i < int(s.size()) - 3; ++i) {
        if (std::isdigit(s[i]) && std::isdigit(s[i + 1]) && std::isdigit(s[i + 2]) &&
            std::isdigit(s[i + 3])) {
            if (i + 4 < int(s.size()) && std::isdigit(s[i + 4])) {
                return NOTHING;
            }
            String mask = s.substr(0, i) + L"%d" + s.substr(i + 4);
            // prepend the original parent path
            return OutputFile(path.parentPath() / Path(mask), firstDumpIdx);
        }
    }
    return NOTHING;
}

bool OutputFile::hasWildcard() const {
    String path = pathMask.string();
    return path.find("%d") != String::npos || path.find("%t") != String::npos;
}

Path OutputFile::getMask() const {
    return pathMask;
}

IOutput::IOutput(const OutputFile& fileMask)
    : paths(fileMask) {
    SPH_ASSERT(!fileMask.getMask().empty());
}

// ----------------------------------------------------------------------------------------------------------
// TextOutput/Input
// ----------------------------------------------------------------------------------------------------------

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
    if (quantities.has(OutputQuantityFlag::INDEX)) {
        columns.push(makeAuto<ParticleNumberColumn>());
    }
    if (quantities.has(OutputQuantityFlag::POSITION)) {
        columns.push(makeAuto<ValueColumn<Vector>>(QuantityId::POSITION));
    }
    if (quantities.has(OutputQuantityFlag::VELOCITY)) {
        columns.push(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITION));
    }
    if (quantities.has(OutputQuantityFlag::ANGULAR_FREQUENCY)) {
        columns.push(makeAuto<ValueColumn<Vector>>(QuantityId::ANGULAR_FREQUENCY));
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
}

struct DumpAllVisitor {
    template <typename TValue>
    void visit(QuantityId id, Array<AutoPtr<ITextColumn>>& columns) {
        columns.push(makeAuto<ValueColumn<TValue>>(id));
    }
};

TextOutput::TextOutput(const OutputFile& fileMask,
    const String& runName,
    const Flags<OutputQuantityFlag> quantities,
    const Flags<Options> options)
    : IOutput(fileMask)
    , runName(runName)
    , options(options) {
    addColumns(quantities, columns);
}

TextOutput::~TextOutput() = default;

Expected<Path> TextOutput::dump(const Storage& storage, const Statistics& stats) {
    if (options.has(Options::DUMP_ALL)) {
        columns.clear();
        // add some 'extraordinary' quantities and position (we want those to be one of the first, not after
        // density, etc).
        columns.push(makeAuto<ParticleNumberColumn>());
        columns.push(makeAuto<ValueColumn<Vector>>(QuantityId::POSITION));
        columns.push(makeAuto<DerivativeColumn<Vector>>(QuantityId::POSITION));
        columns.push(makeAuto<SmoothingLengthColumn>());
        for (ConstStorageElement e : storage.getQuantities()) {
            if (e.id == QuantityId::POSITION) {
                // already added
                continue;
            }
            dispatch(e.quantity.getValueEnum(), DumpAllVisitor{}, e.id, columns);
        }
    }

    SPH_ASSERT(!columns.empty(), "No column added to TextOutput");
    const Path fileName = paths.getNextPath(stats);

    Outcome dirResult = FileSystem::createDirectory(fileName.parentPath());
    if (!dirResult) {
        return makeUnexpected<Path>(
            "Cannot create directory {}: {}", fileName.parentPath().string(), dirResult.error());
    }

    try {
        std::ofstream ofs(fileName.native());
        // print description
        ofs << "# Run: " << runName.toAscii() << std::endl;
        if (stats.has(StatisticsId::RUN_TIME)) {
            ofs << "# SPH dump, time = " << stats.get<Float>(StatisticsId::RUN_TIME) << std::endl;
        }
        ofs << "# ";
        for (const auto& column : columns) {
            std::string asciiName(column->getName().toAscii());
            printHeader(ofs, asciiName, column->getType());
        }
        ofs << std::endl;
        // print data lines, starting with second-order quantities
        for (Size i = 0; i < storage.getParticleCnt(); ++i) {
            for (const auto& column : columns) {
                // write one extra space to be sure numbers won't merge
                if (options.has(Options::SCIENTIFIC)) {
                    ofs << std::scientific << std::setprecision(PRECISION)
                        << column->evaluate(storage, stats, i);
                } else {
                    ofs << std::setprecision(PRECISION) << column->evaluate(storage, stats, i);
                }
            }
            ofs << std::endl;
        }
        ofs.close();
        return fileName;
    } catch (const std::exception& e) {
        return makeUnexpected<Path>("Cannot save output file {}: {}", fileName.string(), exceptionMessage(e));
    }
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
            return makeFailed("Failed to open the file");
        }

        storage.removeAll();
        // storage currently requires at least one quantity for insertion by value
        storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, Array<Size>{ 0 });

        Size particleCnt = 0;
        std::string line;
        while (std::getline(ifs, line)) {
            if (line[0] == '#') { // comment
                continue;
            }
            std::stringstream ss(line);
            for (AutoPtr<ITextColumn>& column : columns) {
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
        Quantity& flags = storage.getQuantity(QuantityId::FLAG);
        for (Array<Size>& buffer : flags.getAll<Size>()) {
            buffer.resize(particleCnt);
        }

        // sanity check
        if (storage.getParticleCnt() != particleCnt || !storage.isValid()) {
            return makeFailed("Loaded storage is not valid");
        }

    } catch (const std::exception& e) {
        return makeFailed(exceptionMessage(e));
    }
    return SUCCESS;
}

TextInput& TextInput::addColumn(AutoPtr<ITextColumn>&& column) {
    columns.push(std::move(column));
    return *this;
}

// ----------------------------------------------------------------------------------------------------------
// BinaryOutput/Input
// ----------------------------------------------------------------------------------------------------------

namespace {

using BufferView = ArrayView<const char>;

struct StoreBuffersVisitor {
    template <typename TValue>
    void visit(const Quantity& q, Serializer<true>& serializer, const IndexSequence& sequence) {
        StaticArray<const Array<TValue>&, 3> buffers = q.getAll<TValue>();
        for (Size i : sequence) {
            serializer.write(buffers[0][i]);
        }
        switch (q.getOrderEnum()) {
        case OrderEnum::ZERO:
            break;
        case OrderEnum::FIRST:
            for (Size i : sequence) {
                serializer.write(buffers[1][i]);
            }
            break;
        case OrderEnum::SECOND:
            for (Size i : sequence) {
                serializer.write(buffers[1][i]);
            }
            for (Size i : sequence) {
                serializer.write(buffers[2][i]);
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
        Deserializer<true>& deserializer,
        const IndexSequence& sequence,
        const QuantityId id,
        const OrderEnum order) {
        Array<TValue> buffer(sequence.size());
        for (Size i : sequence) {
            deserializer.read(buffer[i]);
        }
        storage.insert<TValue>(id, order, std::move(buffer));
        switch (order) {
        case OrderEnum::ZERO:
            // already done
            break;
        case OrderEnum::FIRST: {
            ArrayView<TValue> dv = storage.getDt<TValue>(id);
            for (Size i : sequence) {
                deserializer.read(dv[i]);
            }
            break;
        }
        case OrderEnum::SECOND: {
            ArrayView<TValue> dv = storage.getDt<TValue>(id);
            ArrayView<TValue> d2v = storage.getD2t<TValue>(id);
            for (Size i : sequence) {
                deserializer.read(dv[i]);
            }
            for (Size i : sequence) {
                deserializer.read(d2v[i]);
            }
            break;
        }
        default:
            NOT_IMPLEMENTED;
        }
    }
};

void writeString(const String& s, Serializer<true>& serializer) {
    SPH_ASSERT(s.size() < 16);
    SPH_ASSERT(s.isAscii(), s);
    char buffer[16];
    for (Size i = 0; i < 16; ++i) {
        if (i < s.size()) {
            buffer[i] = char(s[i]);
        } else {
            buffer[i] = '\0';
        }
    }
    serializer.write(buffer);
}

template <bool Precise>
void writeAttractor(Serializer<Precise>& serializer, const Attractor& a) {
    serializer.write(a.position);
    serializer.write(a.velocity);
    serializer.write(a.radius);
    serializer.write(a.mass);

    serializer.write(a.settings.size());
    for (auto param : a.settings) {
        serializer.serialize(param.id);
        serializer.serialize(param.value.getTypeIdx());
        forValue(param.value, [&serializer](const auto& value) { serializer.write(value); });
    }
}

template <bool Precise>
Attractor readAttractor(Deserializer<Precise>& deserializer) {
    Attractor a;
    deserializer.read(a.position);
    deserializer.read(a.velocity);
    deserializer.read(a.radius);
    deserializer.read(a.mass);

    Size paramCnt;
    deserializer.deserialize(paramCnt);
    for (Size i = 0; i < paramCnt; ++i) {
        AttractorSettingsId paramId;
        Size valueId;
        deserializer.deserialize(paramId, valueId);

        SettingsIterator<AttractorSettingsId>::IteratorValue iteratorValue{ paramId,
            { CONSTRUCT_TYPE_IDX, valueId } };

        forValue(iteratorValue.value, [&deserializer, &a, paramId](auto& entry) {
            deserializer.read(entry);
            try {
                setEnumIndex(AttractorSettings::getDefaults(), paramId, entry);
                a.settings.set(paramId, entry);
            } catch (const Exception& UNUSED(e)) {
                // can be a parameter from newer version, silence the exception for backwards compatibility
            }
        });
    }
    return a;
}

} // namespace

BinaryOutput::BinaryOutput(const OutputFile& fileMask, const RunTypeEnum runTypeId)
    : IOutput(fileMask)
    , runTypeId(runTypeId) {}

Expected<Path> BinaryOutput::dump(const Storage& storage, const Statistics& stats) {
    VERBOSE_LOG

    const Path fileName = paths.getNextPath(stats);
    Outcome dirResult = FileSystem::createDirectory(fileName.parentPath());
    if (!dirResult) {
        return makeUnexpected<Path>(
            "Cannot create directory {}: {}", fileName.parentPath().string(), dirResult.error());
    }

    const Float runTime = stats.getOr<Float>(StatisticsId::RUN_TIME, 0._f);
    const Size wallclockTime = stats.getOr<int>(StatisticsId::WALLCLOCK_TIME, 0);

    Serializer<true> serializer(makeAuto<FileBinaryOutputStream>(fileName));

    // file format identifier
    const Size materialCnt = storage.getMaterialCnt();
    const Size quantityCnt = storage.getQuantityCnt() - int(storage.has(QuantityId::MATERIAL_ID));
    const Float timeStep = stats.getOr<Float>(StatisticsId::TIMESTEP_VALUE, 0.1_f);
    serializer.serialize("SPH",
        runTime,
        storage.getParticleCnt(),
        quantityCnt,
        materialCnt,
        timeStep,
        BinaryIoVersion::LATEST);
    // write run type
    writeString(EnumMap::toString(runTypeId), serializer);
    // write build date
    writeString(__DATE__, serializer);
    // write wallclock time for proper ETA of resumed simulation
    serializer.serialize(wallclockTime);
    // number of attractors
    serializer.serialize(storage.getAttractorCnt());

    // zero bytes until 256 to allow extensions of the header
    serializer.addPadding(PADDING_SIZE);

    // quantity information
    Array<QuantityId> cachedIds;
    for (auto i : storage.getQuantities()) {
        // first 3 values: quantity ID, order (number of derivatives), type
        const Quantity& q = i.quantity;
        if (i.id != QuantityId::MATERIAL_ID) {
            // no need to dump material IDs, they are always consecutive
            cachedIds.push(i.id);
            serializer.serialize(Size(i.id), Size(q.getOrderEnum()), Size(q.getValueEnum()));
        }
    }

    const bool hasMaterials = materialCnt > 0;
    // dump quantities separated by materials
    for (Size matIdx = 0; matIdx < max(materialCnt, Size(1)); ++matIdx) {
        // storage can currently exist without materials, only write material params if we have a material
        if (hasMaterials) {
            serializer.serialize("MAT", matIdx);
            MaterialView material = storage.getMaterial(matIdx);
            serializer.serialize(material->getParams().size());
            // dump body settings
            for (auto param : material->getParams()) {
                serializer.serialize(param.id);
                serializer.serialize(param.value.getTypeIdx());
                forValue(param.value, [&serializer](const auto& value) { serializer.write(value); });
            }
            // dump all ranges and minimal values for timestepping
            for (QuantityId id : cachedIds) {
                const Interval range = material->range(id);
                const Float minimal = material->minimal(id);
                serializer.serialize(id, range.lower(), range.upper(), minimal);
            }
        } else {
            // write that we have no materials
            serializer.serialize("NOMAT");
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
        serializer.serialize(*sequence.begin(), *sequence.end());

        for (auto i : storage.getQuantities()) {
            if (i.id != QuantityId::MATERIAL_ID) {
                const Quantity& q = i.quantity;
                StoreBuffersVisitor visitor;
                dispatch(q.getValueEnum(), visitor, q, serializer, sequence);
            }
        }
    }

    // finally dump attractors
    for (const Attractor& a : storage.getAttractors()) {
        writeAttractor(serializer, a);
    }

    return fileName;
}

template <typename TId, typename T>
static void setEnumIndex(const Settings<TId>& UNUSED(settings), const TId UNUSED(paramId), T& UNUSED(entry)) {
    // do nothing for other types
}

template <>
void setEnumIndex(const BodySettings& settings, const BodySettingsId paramId, EnumWrapper& entry) {
    EnumWrapper current = settings.get<EnumWrapper>(paramId);
    entry.index = current.index;
}
template <>
void setEnumIndex(const AttractorSettings& settings, const AttractorSettingsId paramId, EnumWrapper& entry) {
    EnumWrapper current = settings.get<EnumWrapper>(paramId);
    entry.index = current.index;
}


static Expected<Storage> loadMaterial(const Size matIdx,
    Deserializer<true>& deserializer,
    ArrayView<QuantityId> ids,
    const BinaryIoVersion version) {
    Size matIdxCheck;
    String identifier;
    deserializer.deserialize(identifier, matIdxCheck);
    // some consistency checks
    if (identifier != L"MAT") {
        return makeUnexpected<Storage>(L"Invalid material identifier, expected MAT, got " + identifier);
    }
    if (matIdxCheck != matIdx) {
        return makeUnexpected<Storage>(
            L"Unexpected material index, expected {}, got {}, ", matIdx, matIdxCheck);
    }

    Size matParamCnt;
    deserializer.deserialize(matParamCnt);
    BodySettings body;
    for (Size i = 0; i < matParamCnt; ++i) {
        // read body settings
        BodySettingsId paramId;
        Size valueId;
        deserializer.deserialize(paramId, valueId);

        if (version == BinaryIoVersion::FIRST) {
            if (valueId == 1 && body.hasType<EnumWrapper>(paramId)) {
                // enums used to be stored as ints (index 1), now we store it as enum wrapper;
                // convert the value to enum and save manually
                EnumWrapper e = body.get<EnumWrapper>(paramId);
                deserializer.deserialize(e.value);
                body.set(paramId, e);
                continue;
            }
        }

        /// \todo this is currently the only way to access Settings variant, refactor if possible
        SettingsIterator<BodySettingsId>::IteratorValue iteratorValue{ paramId,
            { CONSTRUCT_TYPE_IDX, valueId } };

        forValue(iteratorValue.value, [&deserializer, &body, paramId](auto& entry) {
            deserializer.read(entry);
            // little hack: EnumWrapper is loaded with no type index (as it cannot be serialized), so we have
            // to set it to the correct value, otherwise it would trigger asserts in set function.
            try {
                setEnumIndex(body, paramId, entry);
                body.set(paramId, entry);
            } catch (const Exception& UNUSED(e)) {
                // can be a parameter from newer version, silence the exception for backwards compatibility
                /// \todo report as some warning
            }
        });
    }

    // create material based on settings
    AutoPtr<IMaterial> material = Factory::getMaterial(body);
    // read all ranges and minimal values for timestepping
    for (Size i = 0; i < ids.size(); ++i) {
        QuantityId id;
        Float lower, upper, minimal;
        deserializer.deserialize(id, lower, upper, minimal);
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

static Optional<RunTypeEnum> readRunType(char* buffer, const BinaryIoVersion version) {
    String runTypeStr = String::fromAscii(buffer);
    if (!runTypeStr.empty()) {
        return EnumMap::fromString<RunTypeEnum>(runTypeStr).value();
    } else {
        SPH_ASSERT(version < BinaryIoVersion::V2018_10_24);
        return NOTHING;
    }
}

static Optional<String> readBuildDate(char* buffer, const BinaryIoVersion version) {
    if (version >= BinaryIoVersion::V2021_03_20) {
        return String::fromAscii(buffer);
    } else {
        return NOTHING;
    }
}

Outcome BinaryInput::load(const Path& path, Storage& storage, Statistics& stats) {
    storage.removeAll();
    Deserializer<true> deserializer(makeAuto<FileBinaryInputStream>(path));
    String identifier;
    Float time, timeStep;
    Size wallclockTime;
    Size particleCnt, quantityCnt, materialCnt, attractorCnt;
    BinaryIoVersion version;
    try {
        char runTypeBuffer[16];
        char buildDateBuffer[16];
        deserializer.deserialize(identifier,
            time,
            particleCnt,
            quantityCnt,
            materialCnt,
            timeStep,
            version,
            runTypeBuffer,
            buildDateBuffer,
            wallclockTime,
            attractorCnt);
    } catch (const SerializerException&) {
        return makeFailed("Cannot read file '{}', invalid file format.", path.string());
    } catch (const Exception& e) {
        return makeFailed("Cannot read file '{}'. {}.", path.string(), exceptionMessage(e));
    }

    if (identifier != "SPH") {
        return makeFailed("Invalid format specifier: expected SPH, got " + identifier);
    }
    stats.set(StatisticsId::RUN_TIME, time);
    stats.set(StatisticsId::TIMESTEP_VALUE, timeStep);
    if (version >= BinaryIoVersion::V2021_03_20) {
        stats.set(StatisticsId::WALLCLOCK_TIME, int(wallclockTime));
    }
    if (version < BinaryIoVersion::V2021_08_08) {
        attractorCnt = 0; // there should be zeros anyway, but let's make sure
    }
    try {
        deserializer.skip(BinaryOutput::PADDING_SIZE);
    } catch (SerializerException&) {
        return makeFailed("Incorrect header size");
    }
    Array<QuantityId> quantityIds(quantityCnt);
    Array<OrderEnum> orders(quantityCnt);
    Array<ValueEnum> valueTypes(quantityCnt);
    try {
        for (Size i = 0; i < quantityCnt; ++i) {
            deserializer.deserialize(quantityIds[i], orders[i], valueTypes[i]);
        }
    } catch (SerializerException& e) {
        return makeFailed(exceptionMessage(e));
    }

    // Size loadedQuantities = 0;
    const bool hasMaterials = materialCnt > 0;
    for (Size matIdx = 0; matIdx < max(materialCnt, Size(1)); ++matIdx) {
        Storage bodyStorage;
        if (hasMaterials) {
            try {
                Expected<Storage> loadedStorage = loadMaterial(matIdx, deserializer, quantityIds, version);
                if (!loadedStorage) {
                    return makeFailed(loadedStorage.error());
                } else {
                    bodyStorage = std::move(loadedStorage.value());
                }
            } catch (SerializerException& e) {
                return makeFailed(exceptionMessage(e));
            }
        } else {
            try {
                deserializer.deserialize(identifier);
            } catch (SerializerException& e) {
                return makeFailed(exceptionMessage(e));
            }
            if (identifier != "NOMAT") {
                return makeFailed(
                    "Unexpected missing material identifier, expected NOMAT, got " + identifier);
            }
        }

        try {
            Size from, to;
            deserializer.deserialize(from, to);
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
            return makeFailed(exceptionMessage(e));
        }
        storage.merge(std::move(bodyStorage));
    }
    for (Size i = 0; i < attractorCnt; ++i) {
        const Attractor a = readAttractor(deserializer);
        storage.addAttractor(a);
    }

    return SUCCESS;
}

Expected<BinaryInput::Info> BinaryInput::getInfo(const Path& path) {
    Info info;
    char runTypeBuffer[16];
    char dateBuffer[16];
    String identifier;
    try {
        Deserializer<true> deserializer(makeAuto<FileBinaryInputStream>(path));
        deserializer.deserialize(identifier,
            info.runTime,
            info.particleCnt,
            info.quantityCnt,
            info.materialCnt,
            info.timeStep,
            info.version,
            runTypeBuffer,
            dateBuffer,
            info.wallclockTime,
            info.attractorCnt);
    } catch (SerializerException&) {
        return makeUnexpected<Info>("Cannot read file '{}', invalid file format.", path.string());
    } catch (const Exception& e) {
        return makeUnexpected<Info>("Cannot open file '{}'. {}.", path.string(), exceptionMessage(e));
    }
    if (identifier != "SPH") {
        return makeUnexpected<Info>("Invalid format specifier: expected SPH, got " + identifier);
    }
    info.runType = readRunType(runTypeBuffer, info.version);
    info.buildDate = readBuildDate(dateBuffer, info.version);
    if (info.version < BinaryIoVersion::V2021_08_08) {
        info.attractorCnt = 0;
    }
    return Expected<Info>(std::move(info));
}

// ----------------------------------------------------------------------------------------------------------
// CompressedOutput/Input
// ----------------------------------------------------------------------------------------------------------

CompressedOutput::CompressedOutput(const OutputFile& fileMask,
    const CompressionEnum compression,
    const RunTypeEnum runTypeId)
    : IOutput(fileMask)
    , compression(compression)
    , runTypeId(runTypeId) {}

const int MAGIC_NUMBER = 42;

struct NullOutputStream : public IBinaryOutputStream {
public:
    virtual bool write(ArrayView<const char> UNUSED(buffer)) override {
        return true;
    }
};

template <typename T>
static void compressQuantity(Serializer<false>& serializer,
    const CompressionEnum compression,
    const Array<T>& values) {
    Serializer<false> nullSerializer(makeAuto<NullOutputStream>());

    if (compression == CompressionEnum::RLE) {
        serializer.serialize(MAGIC_NUMBER);

        BufferView lastBuffer = nullptr;
        T lastValue(NAN);
        Size count = 0;
        for (Size i = 0; i < values.size(); ++i) {
            BufferView buffer = nullSerializer.write(values[i]);
            if (buffer != lastBuffer) {
                if (count > 0) {
                    // end of the run, write the count
                    serializer.serialize(count);
                    count = 0;
                }
                lastBuffer = serializer.write(values[i]);
                lastValue = values[i];
            } else {
                if (count == 0) {
                    // first repeated value, write again to mark the start of the run
                    lastBuffer = serializer.write(lastValue);
                }
                ++count;
            }
        }
        // close the last run
        if (count > 0) {
            serializer.serialize(count);
        }
    } else {
        SPH_ASSERT(compression == CompressionEnum::NONE);
        for (Size i = 0; i < values.size(); ++i) {
            serializer.write(values[i]);
        }
    }
}

template <typename T>
static void decompressQuantity(Deserializer<false>& deserializer,
    const CompressionEnum compression,
    Array<T>& values) {
    if (compression == CompressionEnum::RLE) {
        int magic;
        deserializer.deserialize(magic);
        if (magic != MAGIC_NUMBER) {
            throw SerializerException("Invalid compression");
        }

        Optional<T> lastValue = NOTHING;
        Size i = 0;
        while (i < values.size()) {
            deserializer.read(values[i]);
            if (!lastValue || values[i] != lastValue.value()) {
                lastValue = values[i];
                ++i;
            } else {
                Size count;
                deserializer.deserialize(count);
                SPH_ASSERT(i + count <= values.size());
                for (Size j = 0; j < count; ++j) {
                    values[i++] = lastValue.value();
                }
            }
        }
    } else {
        for (Size i = 0; i < values.size(); ++i) {
            deserializer.read(values[i]);
        }
    }
}

Expected<Path> CompressedOutput::dump(const Storage& storage, const Statistics& stats) {
    VERBOSE_LOG

    const Path fileName = paths.getNextPath(stats);
    Outcome dirResult = FileSystem::createDirectory(fileName.parentPath());
    if (!dirResult) {
        return makeUnexpected<Path>(
            "Cannot create directory {}: {}", fileName.parentPath().string(), dirResult.error());
    }

    const Float time = stats.getOr<Float>(StatisticsId::RUN_TIME, 0._f);

    Serializer<false> serializer(makeAuto<FileBinaryOutputStream>(fileName));
    serializer.serialize("CPRSPH", time, storage.getParticleCnt(), compression, CompressedIoVersion::LATEST);

    /// \todo runType as string
    serializer.serialize(runTypeId);
    serializer.serialize(storage.getAttractorCnt());
    serializer.addPadding(226);

    // mandatory, without prefix
    compressQuantity(serializer, compression, storage.getValue<Vector>(QuantityId::POSITION));
    compressQuantity(serializer, compression, storage.getDt<Vector>(QuantityId::POSITION));

    Array<QuantityId> expectedIds{
        QuantityId::MASS, QuantityId::DENSITY, QuantityId::ENERGY, QuantityId::DAMAGE
    };
    Array<QuantityId> ids;
    Size count = 0;
    for (QuantityId id : expectedIds) {
        if (storage.has(id)) {
            ++count;
            ids.push(id);
        }
    }
    serializer.serialize(count);

    for (QuantityId id : ids) {
        serializer.serialize(id);
        compressQuantity(serializer, compression, storage.getValue<Float>(id));
    }

    for (const Attractor& a : storage.getAttractors()) {
        writeAttractor(serializer, a);
    }

    return fileName;
}

Outcome CompressedInput::load(const Path& path, Storage& storage, Statistics& stats) {
    // create any material
    storage = Storage(Factory::getMaterial(BodySettings::getDefaults()));

    Deserializer<false> deserializer(makeAuto<FileBinaryInputStream>(path));
    String identifier;
    Float time;
    Size particleCnt;
    Size attractorCnt;
    CompressedIoVersion version;
    CompressionEnum compression;
    RunTypeEnum runTypeId;
    try {
        deserializer.deserialize(
            identifier, time, particleCnt, compression, version, runTypeId, attractorCnt);
    } catch (SerializerException&) {
        return makeFailed("Cannot read file '{}', invalid file format.", path.string());
    } catch (const Exception& e) {
        return makeFailed("Cannot read file '{}'. {}.", path.string(), exceptionMessage(e));
    }
    if (identifier != "CPRSPH") {
        return makeFailed("Invalid format specifier: expected CPRSPH, got " + identifier);
    }

    if (version < CompressedIoVersion::V2021_08_08) {
        attractorCnt = 0;
    }

    stats.set(StatisticsId::RUN_TIME, time);
    try {
        deserializer.skip(226);
    } catch (SerializerException&) {
        return makeFailed("Incorrect header size");
    }

    try {
        Array<Vector> positions(particleCnt);
        decompressQuantity(deserializer, compression, positions);
        storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(positions));

        Array<Vector> velocities(particleCnt);
        decompressQuantity(deserializer, compression, velocities);
        storage.getDt<Vector>(QuantityId::POSITION) = std::move(velocities);

        Size count;
        deserializer.deserialize(count);
        for (Size i = 0; i < count; ++i) {
            QuantityId id;
            deserializer.deserialize(id);
            Array<Float> values(particleCnt);
            decompressQuantity(deserializer, compression, values);
            storage.insert<Float>(id, OrderEnum::ZERO, std::move(values));
        }

        for (Size i = 0; i < attractorCnt; ++i) {
            Attractor a = readAttractor(deserializer);
            storage.addAttractor(a);
        }
    } catch (SerializerException& e) {
        return makeFailed(exceptionMessage(e));
    }

    SPH_ASSERT(storage.isValid());

    return SUCCESS;
}

Expected<CompressedInput::Info> CompressedInput::getInfo(const Path& path) {
    String identifier;
    Float time;
    Size particleCnt;
    Size attractorCnt;
    CompressedIoVersion version;
    CompressionEnum compression;
    RunTypeEnum runTypeId;
    try {
        Deserializer<false> deserializer(makeAuto<FileBinaryInputStream>(path));
        deserializer.deserialize(
            identifier, time, particleCnt, compression, version, runTypeId, attractorCnt);
    } catch (const SerializerException&) {
        return makeUnexpected<Info>("Cannot read file '{}', invalid file format.", path.string());
    } catch (const Exception& e) {
        return makeUnexpected<Info>("Cannot open file '{}'. {}.", path.string(), exceptionMessage(e));
    }

    if (identifier != "CPRSPH") {
        return makeUnexpected<CompressedInput::Info>(
            "Invalid format specifier: expected CPRSPH, got " + identifier);
    }
    CompressedInput::Info info;
    info.particleCnt = particleCnt;
    info.runTime = time;
    info.runType = runTypeId;
    info.version = version;
    if (version >= CompressedIoVersion::V2021_08_08) {
        info.attractorCnt = attractorCnt;
    } else {
        info.attractorCnt = 0;
    }
    return info;
}

// ----------------------------------------------------------------------------------------------------------
// VtkOutput
// ----------------------------------------------------------------------------------------------------------

static void writeDataArray(std::ofstream& of,
    const Storage& storage,
    const Statistics& stats,
    const ITextColumn& column) {
    switch (column.getType()) {
    case ValueEnum::SCALAR:
        of << R"(      <DataArray type="Float32" Name=")" << column.getName().toAscii()
           << R"(" format="ascii">)";
        break;
    case ValueEnum::VECTOR:
        of << R"(      <DataArray type="Float32" Name=")" << column.getName().toAscii()
           << R"(" NumberOfComponents="3" format="ascii">)";
        break;
    case ValueEnum::INDEX:
        of << R"(      <DataArray type="Int32" Name=")" << column.getName().toAscii()
           << R"(" format="ascii">)";
        break;
    case ValueEnum::SYMMETRIC_TENSOR:
        of << R"(      <DataArray type="Float32" Name=")" << column.getName().toAscii()
           << R"(" NumberOfComponents="6" format="ascii">)";
        break;
    case ValueEnum::TRACELESS_TENSOR:
        of << R"(      <DataArray type="Float32" Name=")" << column.getName().toAscii()
           << R"(" NumberOfComponents="5" format="ascii">)";
        break;
    default:
        NOT_IMPLEMENTED;
    }

    of << "\n";
    for (Size i = 0; i < storage.getParticleCnt(); ++i) {
        of << column.evaluate(storage, stats, i) << "\n";
    }

    of << R"(      </DataArray>)"
       << "\n";
}

VtkOutput::VtkOutput(const OutputFile& fileMask, const Flags<OutputQuantityFlag> flags)
    : IOutput(fileMask)
    , flags(flags) {
    // Positions are stored in <Points> block, other quantities in <PointData>; remove the position flag to
    // avoid storing positions twice
    this->flags.unset(OutputQuantityFlag::POSITION);
}

Expected<Path> VtkOutput::dump(const Storage& storage, const Statistics& stats) {
    VERBOSE_LOG

    const Path fileName = paths.getNextPath(stats);
    Outcome dirResult = FileSystem::createDirectory(fileName.parentPath());
    if (!dirResult) {
        return makeUnexpected<Path>(
            "Cannot create directory {}: {}", fileName.parentPath().string(), dirResult.error());
    }

    try {
        std::ofstream of(fileName.native());
        of << R"(<VTKFile type="UnstructuredGrid" version="0.1" byte_order="LittleEndian">
  <UnstructuredGrid>
    <Piece NumberOfPoints=")"
           << storage.getParticleCnt() << R"(" NumberOfCells="0">
      <Points>
        <DataArray name="Position" type="Float32" NumberOfComponents="3" format="ascii">)"
           << "\n";
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        for (Size i = 0; i < r.size(); ++i) {
            of << r[i] << "\n";
        }
        of << R"(        </DataArray>
      </Points>
      <PointData  Vectors="vector">)"
           << "\n";

        Array<AutoPtr<ITextColumn>> columns;
        addColumns(flags, columns);

        for (auto& column : columns) {
            writeDataArray(of, storage, stats, *column);
        }

        of << R"(      </PointData>
      <Cells>
        <DataArray type="Int32" Name="connectivity" format="ascii">
        </DataArray>
        <DataArray type="Int32" Name="offsets" format="ascii">
        </DataArray>
        <DataArray type="UInt8" Name="types" format="ascii">
        </DataArray>
      </Cells>
    </Piece>
  </UnstructuredGrid>
</VTKFile>)";

        return fileName;
    } catch (const std::exception& e) {
        return makeUnexpected<Path>("Cannot save file {}: {}", fileName.string(), exceptionMessage(e));
    }
}

// ----------------------------------------------------------------------------------------------------------
// Hdf5Input
// ----------------------------------------------------------------------------------------------------------

#ifdef SPH_USE_HDF5

namespace {

// Close handles on every exit path. Explicit closes on the success path also report failures.
class Hdf5Handle : public Noncopyable {
private:
    hid_t id;
    herr_t (*closer)(hid_t);

public:
    Hdf5Handle(const hid_t id, herr_t (*closer)(hid_t), const String& operation)
        : id(id)
        , closer(closer) {
        if (id < 0) {
            throw IoError("HDF5 operation failed: {}", operation);
        }
    }

    ~Hdf5Handle() {
        if (id >= 0) {
            closer(id);
        }
    }

    operator hid_t() const {
        return id;
    }

    void close() {
        if (id >= 0) {
            if (closer(id) < 0) {
                throw IoError("Cannot close HDF5 object");
            }
            id = -1;
        }
    }
};

static void checkHdf5(const herr_t result, const String& operation) {
    if (result < 0) {
        throw IoError("HDF5 operation failed: {}", operation);
    }
}

static String hdf5Name(const std::string& name) {
    return String::fromUtf8(name.c_str());
}

static bool hasHdf5Link(const hid_t parent, const std::string& name) {
    const htri_t exists = H5Lexists(parent, name.c_str(), H5P_DEFAULT);
    checkHdf5(exists, "Check link " + hdf5Name(name));
    return exists > 0;
}

static Array<hsize_t> hdf5Dimensions(const hid_t space) {
    const int rank = H5Sget_simple_extent_ndims(space);
    checkHdf5(rank, "Read dataspace rank");
    const Size dimensions = Size(rank);
    Array<hsize_t> dims(dimensions);
    if (rank > 0) {
        checkHdf5(H5Sget_simple_extent_dims(space, &dims[0], nullptr), "Read dataspace dimensions");
    }
    return dims;
}

static Size hdf5ParticleCount(const hid_t file, const std::string& name) {
    Hdf5Handle dataset(H5Dopen2(file, name.c_str(), H5P_DEFAULT), H5Dclose, "Open " + hdf5Name(name));
    Hdf5Handle space(H5Dget_space(dataset), H5Sclose, "Get position dataspace");
    const Array<hsize_t> dims = hdf5Dimensions(space);
    if (dims.size() != 2 || dims[1] != 3 || dims[0] > NumericLimits<Size>::max() / 3) {
        throw IoError("Invalid position dimensions in '{}' (expected N x 3)", hdf5Name(name));
    }
    const Size count = Size(dims[0]);
    space.close();
    dataset.close();
    return count;
}

template <typename T>
constexpr Size hdf5TypeDim = 1;

template <>
constexpr Size hdf5TypeDim<Vector> = 3;

template <typename T>
static T fromHdf5Buffer(const Array<double>& data, const Size i);

template <>
Float fromHdf5Buffer<Float>(const Array<double>& data, const Size i) {
    return Float(data[i]);
}

template <>
Vector fromHdf5Buffer<Vector>(const Array<double>& data, const Size i) {
    return Vector(Float(data[3 * i]), Float(data[3 * i + 1]), Float(data[3 * i + 2]));
}

template <typename T>
static bool tryLoadQuantity(const hid_t file,
    const std::string& name,
    const QuantityId id,
    const OrderEnum order,
    Storage& storage) {
    if (!hasHdf5Link(file, name)) {
        return false;
    }
    Hdf5Handle dataset(H5Dopen2(file, name.c_str(), H5P_DEFAULT), H5Dclose, "Open " + hdf5Name(name));
    Hdf5Handle space(H5Dget_space(dataset), H5Sclose, "Get dataspace " + hdf5Name(name));
    const Array<hsize_t> dims = hdf5Dimensions(space);
    const Size count = storage.getParticleCnt();
    const Size dim = hdf5TypeDim<T>;
    const bool validShape = dim == 1 ? (dims.size() == 1 || (dims.size() == 2 && dims[1] == 1))
                                     : (dims.size() == 2 && dims[1] == dim);
    if (!validShape || dims[0] != count || count > NumericLimits<Size>::max() / dim) {
        throw IoError("Invalid dataset dimensions in '{}': expected {} particles, {} components",
            hdf5Name(name),
            count,
            dim);
    }
    Array<double> data(dim * count);
    if (count > 0) {
        checkHdf5(H5Dread(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &data[0]),
            "Read " + hdf5Name(name));
    }
    space.close();
    dataset.close();

    Array<T> values(count);
    for (Size i = 0; i < count; ++i) {
        values[i] = fromHdf5Buffer<T>(data, i);
    }
    if (order == OrderEnum::ZERO) {
        storage.insert<T>(id, OrderEnum::ZERO, std::move(values));
    } else {
        storage.getDt<T>(id) = std::move(values);
    }
    return true;
}

static bool
readHdf5Attribute(const hid_t group, const char* name, const hid_t type, void* buffer, const hsize_t count) {
    const htri_t exists = H5Aexists(group, name);
    checkHdf5(exists, "Check attribute " + String::fromAscii(name));
    if (!exists) {
        return false;
    }
    Hdf5Handle attr(H5Aopen(group, name, H5P_DEFAULT), H5Aclose, "Open attribute " + String::fromAscii(name));
    Hdf5Handle space(H5Aget_space(attr), H5Sclose, "Get attribute dataspace");
    const hssize_t points = H5Sget_simple_extent_npoints(space);
    if (points < 0 || hsize_t(points) != count) {
        throw IoError("Invalid number of values in HDF5 attribute '{}'", name);
    }
    checkHdf5(H5Aread(attr, type, buffer), "Read attribute " + String::fromAscii(name));
    space.close();
    attr.close();
    return true;
}

template <typename T>
static void insertHdf5Default(Storage& storage, const QuantityId id, const T value) {
    Array<T> values(storage.getParticleCnt());
    values.fill(value);
    storage.insert<T>(id, OrderEnum::ZERO, std::move(values));
}

static Storage loadHdf5Particles(const hid_t file,
    const std::string& prefix,
    const bool gadget,
    const Size particleType,
    const double constantMass) {
    const std::string position = prefix + (gadget ? "Coordinates" : "x");
    const Size count = hdf5ParticleCount(file, position);
    Storage storage;
    if (count > 0) {
        storage = Storage(Factory::getMaterial(BodySettings::getDefaults()));
    }
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, Array<Vector>(count));
    tryLoadQuantity<Vector>(file, position, QuantityId::POSITION, OrderEnum::ZERO, storage);
    // Missing velocities retain the zero derivative initialized by POSITION.
    tryLoadQuantity<Vector>(
        file, prefix + (gadget ? "Velocities" : "v"), QuantityId::POSITION, OrderEnum::FIRST, storage);

    if (!tryLoadQuantity<Float>(
            file, prefix + (gadget ? "Masses" : "m"), QuantityId::MASS, OrderEnum::ZERO, storage)) {
        if (gadget && count > 0 && (!isReal(constantMass) || constantMass <= 0.)) {
            throw IoError(
                "Missing Masses and a positive Header/MassTable entry for PartType{}", particleType);
        }
        insertHdf5Default<Float>(storage, QuantityId::MASS, gadget ? Float(constantMass) : 1._f);
    }
    struct ScalarField {
        const char* nativeName;
        const char* gadgetName;
        QuantityId id;
        Float fallback;
    };
    for (const ScalarField& field : { ScalarField{ "p", "Pressure", QuantityId::PRESSURE, 0._f },
             ScalarField{ "rho", "Density", QuantityId::DENSITY, 1000._f },
             ScalarField{ "e", "InternalEnergy", QuantityId::ENERGY, 0._f } }) {
        if (!tryLoadQuantity<Float>(file,
                prefix + (gadget ? field.gadgetName : field.nativeName),
                field.id,
                OrderEnum::ZERO,
                storage)) {
            insertHdf5Default<Float>(storage, field.id, field.fallback);
        }
    }
    bool smoothingLoaded = tryLoadQuantity<Float>(file,
        prefix + (gadget ? "SmoothingLength" : "sml"),
        QuantityId::SMOOTHING_LENGTH,
        OrderEnum::ZERO,
        storage);
    if (gadget && !smoothingLoaded) {
        smoothingLoaded = tryLoadQuantity<Float>(
            file, prefix + "SmoothingLengths", QuantityId::SMOOTHING_LENGTH, OrderEnum::ZERO, storage);
    }
    if (!smoothingLoaded) {
        insertHdf5Default<Float>(storage, QuantityId::SMOOTHING_LENGTH, 1._f);
    }
    tryLoadQuantity<Vector>(
        file, prefix + (gadget ? "UVW" : "uvw"), QuantityId::UVW, OrderEnum::ZERO, storage);
    if (gadget) {
        insertHdf5Default<Size>(storage, QuantityId::FLAG, particleType);
    }
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> h = storage.getValue<Float>(QuantityId::SMOOTHING_LENGTH);
    for (Size i = 0; i < count; ++i) {
        r[i][H] = h[i];
    }
    return storage;
}

static void writeHdf5Dataset(const hid_t file,
    const std::string& name,
    const hid_t type,
    const void* buffer,
    const Size count,
    const Size dim = 1) {
    const hsize_t dims[2] = { count, dim };
    Hdf5Handle space(H5Screate_simple(dim == 1 ? 1 : 2, dims, nullptr), H5Sclose, "Create dataspace");
    Hdf5Handle dataset(H5Dcreate2(file, name.c_str(), type, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT),
        H5Dclose,
        "Create " + hdf5Name(name));
    if (count > 0) {
        checkHdf5(H5Dwrite(dataset, type, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer), "Write " + hdf5Name(name));
    }
    dataset.close();
    space.close();
}

static void writeHdf5Attribute(const hid_t group,
    const char* name,
    const hid_t type,
    const void* buffer,
    const hsize_t count = 0) {
    Hdf5Handle space(count ? H5Screate_simple(1, &count, nullptr) : H5Screate(H5S_SCALAR),
        H5Sclose,
        "Create attribute dataspace");
    Hdf5Handle attr(H5Acreate2(group, name, type, space, H5P_DEFAULT, H5P_DEFAULT),
        H5Aclose,
        "Create attribute " + String::fromAscii(name));
    checkHdf5(H5Awrite(attr, type, buffer), "Write attribute " + String::fromAscii(name));
    attr.close();
    space.close();
}

static void setHdf5Buffer(Array<double>& buffer, const Float value, const Size i) {
    buffer[i] = double(value);
}

static void setHdf5Buffer(Array<double>& buffer, const Vector& value, const Size i) {
    buffer[3 * i] = double(value[X]);
    buffer[3 * i + 1] = double(value[Y]);
    buffer[3 * i + 2] = double(value[Z]);
}

template <typename T>
static bool saveQuantity(const hid_t file,
    const std::string& name,
    const QuantityId id,
    const OrderEnum order,
    const Storage& storage) {
    if (!storage.has(id) || storage.getQuantity(id).getOrderEnum() < order) {
        return false;
    }
    const ArrayView<const T> values =
        order == OrderEnum::ZERO ? storage.getValue<T>(id) : storage.getDt<T>(id);
    const Size count = storage.getParticleCnt();
    const Size dim = hdf5TypeDim<T>;
    if (values.size() != count || count > NumericLimits<Size>::max() / dim) {
        throw IoError("Invalid buffer size for '{}'", hdf5Name(name));
    }
    Array<double> buffer(dim * count);
    for (Size i = 0; i < count; ++i) {
        setHdf5Buffer(buffer, values[i], i);
    }
    writeHdf5Dataset(file, name, H5T_NATIVE_DOUBLE, count ? &buffer[0] : nullptr, count, dim);
    return true;
}

static void saveHdf5Particles(const hid_t file, const Storage& storage, const bool gadget) {
    const std::string prefix = gadget ? "/PartType0/" : "/";
    if (!saveQuantity<Vector>(
            file, prefix + (gadget ? "Coordinates" : "x"), QuantityId::POSITION, OrderEnum::ZERO, storage)) {
        throw IoError("Cannot export HDF5 particles without positions");
    }
    if (!saveQuantity<Vector>(
            file, prefix + (gadget ? "Velocities" : "v"), QuantityId::POSITION, OrderEnum::FIRST, storage) &&
        gadget) {
        Array<double> zeroVelocities(3 * storage.getParticleCnt());
        zeroVelocities.fill(0.);
        writeHdf5Dataset(file,
            prefix + "Velocities",
            H5T_NATIVE_DOUBLE,
            zeroVelocities.empty() ? nullptr : &zeroVelocities[0],
            storage.getParticleCnt(),
            3);
    }
    if (!saveQuantity<Float>(
            file, prefix + (gadget ? "Masses" : "m"), QuantityId::MASS, OrderEnum::ZERO, storage) &&
        gadget) {
        throw IoError("Cannot export GADGET particles without masses");
    }
    saveQuantity<Float>(
        file, prefix + (gadget ? "Pressure" : "p"), QuantityId::PRESSURE, OrderEnum::ZERO, storage);
    saveQuantity<Float>(
        file, prefix + (gadget ? "Density" : "rho"), QuantityId::DENSITY, OrderEnum::ZERO, storage);
    if (!saveQuantity<Float>(
            file, prefix + (gadget ? "InternalEnergy" : "e"), QuantityId::ENERGY, OrderEnum::ZERO, storage) &&
        gadget) {
        Array<double> energy(storage.getParticleCnt());
        energy.fill(0.);
        writeHdf5Dataset(file,
            prefix + "InternalEnergy",
            H5T_NATIVE_DOUBLE,
            energy.empty() ? nullptr : &energy[0],
            energy.size());
    }
    saveQuantity<Vector>(file, prefix + (gadget ? "UVW" : "uvw"), QuantityId::UVW, OrderEnum::ZERO, storage);
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    Array<double> smoothing(r.size());
    for (Size i = 0; i < r.size(); ++i) {
        smoothing[i] = double(r[i][H]);
    }
    writeHdf5Dataset(file,
        prefix + (gadget ? "SmoothingLength" : "sml"),
        H5T_NATIVE_DOUBLE,
        smoothing.empty() ? nullptr : &smoothing[0],
        smoothing.size());
    if (gadget) {
        Array<uint64_t> ids(r.size());
        for (Size i = 0; i < ids.size(); ++i) {
            ids[i] = uint64_t(i) + 1;
        }
        writeHdf5Dataset(
            file, prefix + "ParticleIDs", H5T_NATIVE_UINT64, ids.empty() ? nullptr : &ids[0], ids.size());
    }
}

static void saveGadgetHeader(const hid_t file, const Size count, const double time) {
    Hdf5Handle header(
        H5Gcreate2(file, "/Header", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT), H5Gclose, "Create Header");
    const uint32_t counts[6] = { count, 0, 0, 0, 0, 0 };
    const uint32_t highWords[6] = {};
    const double masses[6] = {}; // All masses are stored in the Masses dataset.
    writeHdf5Attribute(header, "NumPart_ThisFile", H5T_NATIVE_UINT32, counts, 6);
    writeHdf5Attribute(header, "NumPart_Total", H5T_NATIVE_UINT32, counts, 6);
    writeHdf5Attribute(header, "NumPart_Total_HighWord", H5T_NATIVE_UINT32, highWords, 6);
    // The original GADGET-2 reader uses this older spelling.
    writeHdf5Attribute(header, "NumPart_Total_HW", H5T_NATIVE_UINT32, highWords, 6);
    writeHdf5Attribute(header, "MassTable", H5T_NATIVE_DOUBLE, masses, 6);
    writeHdf5Attribute(header, "Time", H5T_NATIVE_DOUBLE, &time);
    const double zero = 0., one = 1.;
    // Non-periodic, non-cosmological export; do not invent a cosmology or box size.
    for (const char* name : { "BoxSize", "Redshift", "Omega0", "OmegaLambda" }) {
        writeHdf5Attribute(header, name, H5T_NATIVE_DOUBLE, &zero);
    }
    writeHdf5Attribute(header, "HubbleParam", H5T_NATIVE_DOUBLE, &one);
    const int disabled = 0, enabled = 1;
    writeHdf5Attribute(header, "NumFilesPerSnapshot", H5T_NATIVE_INT, &enabled);
    writeHdf5Attribute(header, "Flag_DoublePrecision", H5T_NATIVE_INT, &enabled);
    for (const char* name : { "Flag_Sfr",
             "Flag_Feedback",
             "Flag_Cooling",
             "Flag_StellarAge",
             "Flag_Metals",
             "Flag_Entropy_ICs" }) {
        writeHdf5Attribute(header, name, H5T_NATIVE_INT, &disabled);
    }
    header.close();
}

static Expected<Path>
dumpHdf5(const Path& path, const Storage& storage, const Statistics& stats, const bool gadget) {
    const Outcome dirResult = FileSystem::createDirectory(path.parentPath());
    if (!dirResult) {
        return makeUnexpected<Path>(
            "Cannot create directory {}: {}", path.parentPath().string(), dirResult.error());
    }
    try {
        Hdf5Handle file(H5Fcreate(path.string().toUtf8(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT),
            H5Fclose,
            "Create file " + path.string());
        const double time = double(stats.getOr<Float>(StatisticsId::RUN_TIME, 0._f));
        if (gadget) {
            saveGadgetHeader(file, storage.getParticleCnt(), time);
            Hdf5Handle group(H5Gcreate2(file, "/PartType0", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT),
                H5Gclose,
                "Create PartType0");
            saveHdf5Particles(file, storage, true);
            group.close();
        } else {
            writeHdf5Dataset(file, "/time", H5T_NATIVE_DOUBLE, &time, 1);
            saveHdf5Particles(file, storage, false);
        }
        checkHdf5(H5Fflush(file, H5F_SCOPE_LOCAL), "Flush output file");
        file.close();
        return path;
    } catch (const std::exception& e) {
        return makeUnexpected<Path>("Cannot save HDF5 file '{}': {}", path.string(), exceptionMessage(e));
    }
}

} // namespace

Outcome Hdf5Input::load(const Path& path, Storage& storage, Statistics& stats) {
    try {
        Hdf5Handle file(H5Fopen(path.string().toUtf8(), H5F_ACC_RDONLY, H5P_DEFAULT),
            H5Fclose,
            "Open file " + path.string());
        Array<Size> particleTypes;
        for (Size type = 0; type < 6; ++type) {
            if (hasHdf5Link(file, "/PartType" + std::to_string(type))) {
                particleTypes.push(type);
            }
        }
        const bool gadget = !particleTypes.empty();
        double time = 0., masses[6] = {};
        if (gadget && hasHdf5Link(file, "/Header")) {
            Hdf5Handle header(H5Gopen2(file, "/Header", H5P_DEFAULT), H5Gclose, "Open Header");
            readHdf5Attribute(header, "Time", H5T_NATIVE_DOUBLE, &time, 1);
            readHdf5Attribute(header, "MassTable", H5T_NATIVE_DOUBLE, masses, 6);
            int files = 1;
            readHdf5Attribute(header, "NumFilesPerSnapshot", H5T_NATIVE_INT, &files, 1);
            if (files != 1) {
                throw IoError(
                    "Multi-file GADGET snapshots are not supported (NumFilesPerSnapshot = {})", files);
            }
            header.close();
        }
        if (hasHdf5Link(file, "/time")) {
            Hdf5Handle dataset(H5Dopen2(file, "/time", H5P_DEFAULT), H5Dclose, "Open time");
            Hdf5Handle space(H5Dget_space(dataset), H5Sclose, "Get time dataspace");
            if (H5Sget_simple_extent_npoints(space) != 1) {
                throw IoError("Invalid time dataset: expected one value");
            }
            checkHdf5(H5Dread(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &time), "Read time");
            space.close();
            dataset.close();
        }
        Storage loaded;
        if (gadget) {
            for (const Size type : particleTypes) {
                Storage group = loadHdf5Particles(
                    file, "/PartType" + std::to_string(type) + "/", true, type, masses[type]);
                if (group.empty()) {
                    continue;
                }
                if (group.getParticleCnt() > NumericLimits<Size>::max() / 3 - loaded.getParticleCnt()) {
                    throw IoError("Too many particles in GADGET snapshot");
                }
                loaded.merge(std::move(group));
            }
            if (loaded.getQuantityCnt() == 0) {
                loaded = loadHdf5Particles(file,
                    "/PartType" + std::to_string(particleTypes[0]) + "/",
                    true,
                    particleTypes[0],
                    masses[particleTypes[0]]);
            }
        } else {
            loaded = loadHdf5Particles(file, "/", false, 0, 0.);
        }
        file.close();
        storage = std::move(loaded);
        stats.set(StatisticsId::RUN_TIME, Float(time));
        return SUCCESS;
    } catch (const std::exception& e) {
        return makeFailed("Cannot read HDF5 file '{}': {}", path.string(), exceptionMessage(e));
    }
}

Hdf5Output::Hdf5Output(const OutputFile& fileMask)
    : IOutput(fileMask) {}

Expected<Path> Hdf5Output::dump(const Storage& storage, const Statistics& stats) {
    return dumpHdf5(paths.getNextPath(stats), storage, stats, false);
}

GadgetHdf5Output::GadgetHdf5Output(const OutputFile& fileMask)
    : IOutput(fileMask) {}

Expected<Path> GadgetHdf5Output::dump(const Storage& storage, const Statistics& stats) {
    return dumpHdf5(paths.getNextPath(stats), storage, stats, true);
}

#else

Outcome Hdf5Input::load(const Path&, Storage&, Statistics&) {
    return makeFailed("HDF5 support not enabled. Please rebuild the code with CMake option -DWITH_HDF5=ON.");
}

Hdf5Output::Hdf5Output(const OutputFile& fileMask)
    : IOutput(fileMask) {}

Expected<Path> Hdf5Output::dump(const Storage&, const Statistics&) {
    return makeUnexpected<Path>("HDF5 support not enabled. Please rebuild the code with CMake option -DWITH_HDF5=ON.");
}

GadgetHdf5Output::GadgetHdf5Output(const OutputFile& fileMask)
    : IOutput(fileMask) {}

Expected<Path> GadgetHdf5Output::dump(const Storage&, const Statistics&) {
    return makeUnexpected<Path>("HDF5 support not enabled. Please rebuild the code with CMake option -DWITH_HDF5=ON.");
}
#endif

// ----------------------------------------------------------------------------------------------------------
// MpcorpInput
// ----------------------------------------------------------------------------------------------------------

static Float computeRadius(const Float H, const Float albedo) {
    // https://cneos.jpl.nasa.gov/tools/ast_size_est.html
    const Float d = exp10(3.1236_f - 0.5_f * log10(albedo) - 0.2_f * H);
    return 0.5_f * d * 1.e3_f;
}

static void parseMpcorp(std::ifstream& ifs, Storage& storage, const Float rho, const Float albedo) {
    std::string line;
    // skip header
    while (std::getline(ifs, line)) {
        if (line.size() >= 5 && line.substr(0, 5) == "-----") {
            break;
        }
    }

    std::string dummy;
    Array<Vector> positions, velocities;
    Array<Float> masses;
    Array<Size> flags;
    while (std::getline(ifs, line)) {
        if (line.empty()) {
            continue;
        }
        std::stringstream ss(line);
        Float mag;
        ss >> dummy >> mag >> dummy >> dummy;
        if (!ss.good()) {
            continue;
        }
        Float M, omega, Omega, I, e, n, a;
        ss >> M >> omega >> Omega >> I >> e >> n >> a;
        M *= DEG_TO_RAD;
        omega *= DEG_TO_RAD;
        Omega *= DEG_TO_RAD;
        I *= DEG_TO_RAD;
        a *= Constants::au;
        n *= DEG_TO_RAD / Constants::day;
        std::string flag;
        ss >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> flag;

        const Float E = Kepler::solveKeplersEquation(M, e);
        const AffineMatrix R_Omega = AffineMatrix::rotateZ(Omega);
        const AffineMatrix R_I = AffineMatrix::rotateX(I);
        const AffineMatrix R_omega = AffineMatrix::rotateZ(omega);
        const AffineMatrix R = R_Omega * R_I * R_omega;

        Vector r = a * R * Vector(cos(E) - e, sqrt(1 - sqr(e)) * sin(E), 0);
        SPH_ASSERT(isReal(r), r);
        Vector v = a * R * n / (1 - e * cos(E)) * Vector(-sin(E), sqrt(1 - sqr(e)) * cos(E), 0);
        SPH_ASSERT(isReal(v), v);
        r[H] = computeRadius(mag, albedo);
        v[H] = 0._f;
        positions.push(r);
        velocities.push(v);

        const Float m = sphereVolume(r[H]) * rho;
        masses.push(m);

        if (std::isdigit(flag.back())) {
            flags.push(flag.back() - '0');
        } else {
            flags.push(0);
        }
    }

    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(positions));
    storage.getDt<Vector>(QuantityId::POSITION) = std::move(velocities);
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, std::move(masses));
    storage.insert<Size>(QuantityId::FLAG, OrderEnum::ZERO, std::move(flags));
}

Outcome MpcorpInput::load(const Path& path, Storage& storage, Statistics& UNUSED(stats)) {
    try {
        std::ifstream ifs(path.native());
        if (!ifs) {
            return makeFailed("Failed to open file '{}'", path.string());
        }
        parseMpcorp(ifs, storage, rho, albedo);
        return SUCCESS;
    } catch (const std::exception& e) {
        return makeFailed("Cannot load file '{}'\n{}", path.string(), exceptionMessage(e));
    }
}


// ----------------------------------------------------------------------------------------------------------
// PkdgravOutput/Input
// ----------------------------------------------------------------------------------------------------------

PkdgravOutput::PkdgravOutput(const OutputFile& fileMask, PkdgravParams&& params)
    : IOutput(fileMask)
    , params(std::move(params)) {
    SPH_ASSERT(almostEqual(this->params.conversion.velocity, 2.97853e4_f, 1.e-4_f));
}

Expected<Path> PkdgravOutput::dump(const Storage& storage, const Statistics& stats) {
    const Path fileName = paths.getNextPath(stats);
    FileSystem::createDirectory(fileName.parentPath());

    ArrayView<const Float> m, rho, u;
    tie(m, rho, u) = storage.getValues<Float>(QuantityId::MASS, QuantityId::DENSITY, QuantityId::ENERGY);
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    ArrayView<const Size> flags = storage.getValue<Size>(QuantityId::FLAG);

    std::ofstream ofs(fileName.native());
    ofs << std::setprecision(PRECISION) << std::scientific;

    Size idx = 0;
    for (Size i = 0; i < r.size(); ++i) {
        if (u[i] > params.vaporThreshold) {
            continue;
        }
        const Float radius = this->getRadius(r[idx][H], m[idx], rho[idx]);
        const Vector v_in = v[idx] + cross(params.omega, r[idx]);
        SPH_ASSERT(flags[idx] < params.colors.size(), flags[idx], params.colors.size());
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

        virtual String getName() const override {
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
    SPH_ASSERT(storage.has<Vector>(QuantityId::POSITION, OrderEnum::FIRST));
    storage.getQuantity(QuantityId::POSITION).setOrder(OrderEnum::SECOND);

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
        r[i][H] = root<3>(3._f * m[i] / (2700._f * 4._f * PI));

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

// ----------------------------------------------------------------------------------------------------------
// TabInput
// ----------------------------------------------------------------------------------------------------------

TabInput::TabInput() {
    input = makeAuto<TextInput>(
        OutputQuantityFlag::MASS | OutputQuantityFlag::POSITION | OutputQuantityFlag::VELOCITY);
}

TabInput::~TabInput() = default;

Outcome TabInput::load(const Path& path, Storage& storage, Statistics& stats) {
    Outcome result = input->load(path, storage, stats);
    if (!result) {
        return result;
    }

    storage.getQuantity(QuantityId::POSITION).setOrder(OrderEnum::SECOND);
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        r[i][H] = 1.e-5_f;
    }

    return SUCCESS;
}

NAMESPACE_SPH_END
