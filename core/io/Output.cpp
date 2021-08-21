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
            buffer[i] = s[i];
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
                a.settings.set(paramId, entry);
            } catch (const Exception& UNUSED(e)) {
                // can be a parameter from newer version, silence the exception for backwards compatibility
            }
        });
        // needs to be handled the same way as BodySettings if Enums are ever included in the attractor
        SPH_ASSERT(!a.settings.hasType<EnumWrapper>(paramId));
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

template <typename T>
static void setEnumIndex(const BodySettings& UNUSED(settings),
    const BodySettingsId UNUSED(paramId),
    T& UNUSED(entry)) {
    // do nothing for other types
}

template <>
void setEnumIndex(const BodySettings& settings, const BodySettingsId paramId, EnumWrapper& entry) {
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

template <typename T>
Size typeDim;

template <>
Size typeDim<Float> = 1;
template <>
Size typeDim<Vector> = 3;

template <typename T>
T doubleToType(ArrayView<const double> data, const Size i);

template <>
Float doubleToType(ArrayView<const double> data, const Size i) {
    return Float(data[i]);
}
template <>
Vector doubleToType(ArrayView<const double> data, const Size i) {
    return Vector(Float(data[3 * i + 0]), Float(data[3 * i + 1]), Float(data[3 * i + 2]));
}

template <typename T>
static void loadQuantity(const hid_t fileId,
    const std::string& label,
    const QuantityId id,
    const OrderEnum order,
    Storage& storage) {
    const hid_t hid = H5Dopen(fileId, label.c_str(), H5P_DEFAULT);
    if (hid < 0) {
        throw IoError("Cannot read " + getMetadata(id).quantityName + " data");
    }
    const Size particleCnt = storage.getParticleCnt();
    Array<double> data(typeDim<T> * particleCnt);
    H5Dread(hid, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &data[0]);
    H5Dclose(hid);

    Array<T> values(particleCnt);
    for (Size i = 0; i < particleCnt; ++i) {
        values[i] = doubleToType<T>(data, i);
    }
    switch (order) {
    case OrderEnum::ZERO:
        storage.insert<T>(id, OrderEnum::ZERO, std::move(values));
        break;
    case OrderEnum::FIRST:
        storage.getDt<T>(id) = std::move(values);
        break;
    case OrderEnum::SECOND:
        storage.getD2t<T>(id) = std::move(values);
        break;
    default:
        NOT_IMPLEMENTED;
    }
}

Outcome Hdf5Input::load(const Path& path, Storage& storage, Statistics& stats) {
    const hid_t fileId = H5Fopen(path.native(), H5F_ACC_RDONLY, H5P_DEFAULT);
    if (fileId < 0) {
        return makeFailed("Cannot open file '{}'", path.string());
    }

    storage = Storage(Factory::getMaterial(BodySettings::getDefaults()));

    const hid_t posId = H5Dopen(fileId, "/x", H5P_DEFAULT);
    if (posId < 0) {
        return makeFailed("Cannot read position data from file '{}'", path.string());
    }
    const hid_t dspace = H5Dget_space(posId);
    const Size ndims = H5Sget_simple_extent_ndims(dspace);
    Array<hsize_t> dims(ndims);
    H5Sget_simple_extent_dims(dspace, &dims[0], nullptr);
    const Size particleCnt = dims[0];
    H5Dclose(posId);
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, Array<Vector>(particleCnt));

    const hid_t timeId = H5Dopen(fileId, "/time", H5P_DEFAULT);
    if (timeId < 0) {
        return makeFailed("Cannot read simulation time from file '{}'", path.string());
    }
    double runTime;
    H5Dread(timeId, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &runTime);
    H5Dclose(timeId);
    stats.set(StatisticsId::RUN_TIME, Float(runTime));

    try {
        loadQuantity<Vector>(fileId, "/x", QuantityId::POSITION, OrderEnum::ZERO, storage);
        loadQuantity<Vector>(fileId, "/v", QuantityId::POSITION, OrderEnum::FIRST, storage);
        loadQuantity<Float>(fileId, "/m", QuantityId::MASS, OrderEnum::ZERO, storage);
        loadQuantity<Float>(fileId, "/p", QuantityId::PRESSURE, OrderEnum::ZERO, storage);
        loadQuantity<Float>(fileId, "/rho", QuantityId::DENSITY, OrderEnum::ZERO, storage);
        loadQuantity<Float>(fileId, "/e", QuantityId::ENERGY, OrderEnum::ZERO, storage);
        loadQuantity<Float>(fileId, "/sml", QuantityId::SMOOTHING_LENGTH, OrderEnum::ZERO, storage);
    } catch (const IoError& e) {
        return makeFailed("Cannot read file '{}'.\n{}", path.string(), exceptionMessage(e));
    }

    // copy the smoothing lengths
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<Float> h = storage.getValue<Float>(QuantityId::SMOOTHING_LENGTH);
    for (Size i = 0; i < particleCnt; ++i) {
        r[i][H] = h[i];
    }
    return SUCCESS;
}


#else

Outcome Hdf5Input::load(const Path&, Storage&, Statistics&) {
    return makeFailed("HDF5 support not enabled. Please rebuild the code with CONFIG+=use_hdf5.");
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
