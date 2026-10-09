#include "catch.hpp"
#include "io/FileManager.h"
#include "io/FileSystem.h"
#include "io/Output.h"
#include "objects/utility/EnumMap.h"
#include "quantities/Quantity.h"
#include "run/IRun.h"
#include "run/jobs/SimulationJobs.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "tests/Setup.h"
#include <vector>

#ifdef SPH_USE_HDF5
#include <hdf5.h>
#ifndef SPH_WIN
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#endif

using namespace Sph;

TEST_CASE("HDF5 capabilities match compiled support", "[hdf5]") {
    for (const IoEnum format : { IoEnum::HDF5_FILE, IoEnum::GADGET_HDF5_FILE }) {
        bool registered = false;
        for (const IoEnum available : EnumMap::getAll<IoEnum>()) {
            registered |= available == format;
        }
#ifdef SPH_USE_HDF5
        REQUIRE(registered);
        REQUIRE(getIoCapabilities(format).has(IoCapability::INPUT));
        REQUIRE(getIoCapabilities(format).has(IoCapability::OUTPUT));
#else
        REQUIRE_FALSE(registered);
        REQUIRE(getIoCapabilities(format) == EMPTY_FLAGS);
#endif
    }
#ifndef SPH_USE_HDF5
    Storage storage;
    Statistics stats;
    Hdf5Input input;
    REQUIRE_FALSE(input.load(Path("unsupported.h5"), storage, stats));
    Hdf5Output native(Path("unsupported.h5"));
    GadgetHdf5Output gadget(Path("unsupported.h5"));
    REQUIRE_FALSE(native.dump(storage, stats));
    REQUIRE_FALSE(gadget.dump(storage, stats));
#endif
}

#ifdef SPH_USE_HDF5

namespace {

class Hdf5TestFile {
public:
    Path path;
    Path directory;
    H5E_auto2_t errorHandler;
    void* errorData = nullptr;

    explicit Hdf5TestFile(const bool unicode = false) {
        RandomPathManager manager;
        path = manager.getPath("h5");
        if (unicode) {
            directory = Path(L"hdf5_\u03b1_" + path.string());
            REQUIRE(FileSystem::createDirectory(directory));
            path = directory / Path(L"snapshot_\u03b2.h5");
        }
        REQUIRE(H5Eget_auto2(H5E_DEFAULT, &errorHandler, &errorData) >= 0);
        REQUIRE(H5Eset_auto2(H5E_DEFAULT, nullptr, nullptr) >= 0);
    }

    ~Hdf5TestFile() {
        H5Eset_auto2(H5E_DEFAULT, errorHandler, errorData);
        if (FileSystem::pathExists(path)) {
            FileSystem::removePath(path);
        }
        if (!directory.empty()) {
            FileSystem::removePath(directory);
        }
    }
};

static hid_t createTestFile(const Path& path) {
    const hid_t file = H5Fcreate(path.string().toUtf8(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    REQUIRE(file >= 0);
    return file;
}

static void addDataset(const hid_t file,
    const std::string& name,
    const std::vector<hsize_t>& dims,
    const std::vector<double>& data) {
    const hid_t space =
        dims.empty() ? H5Screate(H5S_SCALAR) : H5Screate_simple(int(dims.size()), dims.data(), nullptr);
    REQUIRE(space >= 0);
    const hid_t dataset =
        H5Dcreate2(file, name.c_str(), H5T_NATIVE_DOUBLE, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    REQUIRE(dataset >= 0);
    if (!data.empty()) {
        REQUIRE(H5Dwrite(dataset, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, data.data()) >= 0);
    }
    REQUIRE(H5Dclose(dataset) >= 0);
    REQUIRE(H5Sclose(space) >= 0);
}

static void addAttribute(const hid_t group, const char* name, const std::vector<double>& values) {
    const hsize_t count = values.size();
    const hid_t space = H5Screate_simple(1, &count, nullptr);
    REQUIRE(space >= 0);
    const hid_t attr = H5Acreate2(group, name, H5T_NATIVE_DOUBLE, space, H5P_DEFAULT, H5P_DEFAULT);
    REQUIRE(attr >= 0);
    REQUIRE(H5Awrite(attr, H5T_NATIVE_DOUBLE, values.data()) >= 0);
    REQUIRE(H5Aclose(attr) >= 0);
    REQUIRE(H5Sclose(space) >= 0);
}

static Storage hdf5TestStorage(const Size count = 3) {
    Storage storage;
    Array<Vector> positions(count), velocities(count), uvw(count);
    Array<Float> masses(count);
    for (Size i = 0; i < count; ++i) {
        positions[i] = Vector(Float(i), Float(i + 1), -Float(i + 2), 0.25_f + Float(i));
        velocities[i] = Vector(-Float(i), 2._f, 3._f);
        uvw[i] = Vector(0.1_f * i, 0.2_f * i, 0.3_f * i);
        masses[i] = 2._f + i;
    }
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(positions));
    storage.getDt<Vector>(QuantityId::POSITION) = std::move(velocities);
    storage.insert<Vector>(QuantityId::UVW, OrderEnum::ZERO, std::move(uvw));
    storage.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, std::move(masses));
    for (const QuantityId id : { QuantityId::PRESSURE, QuantityId::DENSITY, QuantityId::ENERGY }) {
        Array<Float> values(count);
        values.fill(id == QuantityId::PRESSURE ? 3._f : id == QuantityId::DENSITY ? 4._f : 5._f);
        storage.insert<Float>(id, OrderEnum::ZERO, std::move(values));
    }
    return storage;
}

static Expected<Path>
dumpTestFile(const bool gadget, const Path& path, const Storage& storage, const Statistics& stats) {
    if (gadget) {
        GadgetHdf5Output output(path);
        return output.dump(storage, stats);
    }
    Hdf5Output output(path);
    return output.dump(storage, stats);
}

static void requireFailedLoad(const Path& path) {
    Storage storage = hdf5TestStorage(1);
    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 123._f);
    const ssize_t handles = H5Fget_obj_count(H5F_OBJ_ALL, H5F_OBJ_ALL);
    Hdf5Input input;
    REQUIRE_FALSE(input.load(path, storage, stats));
    REQUIRE(storage.getParticleCnt() == 1);
    REQUIRE(storage.getValue<Float>(QuantityId::MASS)[0] == 2._f);
    REQUIRE(stats.get<Float>(StatisticsId::RUN_TIME) == 123._f);
    REQUIRE(H5Fget_obj_count(H5F_OBJ_ALL, H5F_OBJ_ALL) == handles);
}

} // namespace

TEST_CASE("Native and GADGET HDF5 round trips", "[hdf5]") {
    for (const bool gadget : { false, true }) {
        for (const bool unicode : { false, true }) {
            INFO("GADGET=" << gadget << ", Unicode=" << unicode);
            Hdf5TestFile file(unicode);
            Storage source = hdf5TestStorage();
            Statistics stats;
            stats.set(StatisticsId::RUN_TIME, 12.5_f);
            const Expected<Path> dumped = dumpTestFile(gadget, file.path, source, stats);
            REQUIRE(dumped);
            REQUIRE(dumped.value() == file.path);
            REQUIRE(FileSystem::pathExists(file.path));
            Statistics loadedStats;
            Storage loaded;
            Hdf5Input input;
            REQUIRE(input.load(file.path, loaded, loadedStats));
            REQUIRE(loaded.isValid());
            REQUIRE(loaded.getParticleCnt() == source.getParticleCnt());
            REQUIRE(loadedStats.get<Float>(StatisticsId::RUN_TIME) == 12.5_f);
            for (Size i = 0; i < source.getParticleCnt(); ++i) {
                REQUIRE(almostEqual(loaded.getValue<Vector>(QuantityId::POSITION)[i],
                    source.getValue<Vector>(QuantityId::POSITION)[i]));
                REQUIRE(almostEqual(loaded.getDt<Vector>(QuantityId::POSITION)[i],
                    source.getDt<Vector>(QuantityId::POSITION)[i]));
                REQUIRE(almostEqual(loaded.getValue<Vector>(QuantityId::UVW)[i],
                    source.getValue<Vector>(QuantityId::UVW)[i]));
                for (const QuantityId id :
                    { QuantityId::MASS, QuantityId::PRESSURE, QuantityId::DENSITY, QuantityId::ENERGY }) {
                    REQUIRE(loaded.getValue<Float>(id)[i] == source.getValue<Float>(id)[i]);
                }
                REQUIRE(loaded.getValue<Float>(QuantityId::SMOOTHING_LENGTH)[i] ==
                        source.getValue<Vector>(QuantityId::POSITION)[i][H]);
            }
        }
    }
}

TEST_CASE("HDF5 accepts empty snapshots", "[hdf5]") {
    for (const bool gadget : { false, true }) {
        Hdf5TestFile file;
        Storage source = hdf5TestStorage(0), loaded;
        Statistics stats;
        REQUIRE(dumpTestFile(gadget, file.path, source, stats));
        Hdf5Input input;
        REQUIRE(input.load(file.path, loaded, stats));
        REQUIRE(loaded.empty());
        REQUIRE(loaded.isValid());
    }
}

TEST_CASE("SPH HDF5 output interval also caps the initial timestep", "[hdf5][run]") {
    Hdf5TestFile file;
    RunSettings settings;
    settings.set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::HDF5_FILE);
    settings.set(RunSettingsId::RUN_OUTPUT_PATH, L""_s);
    settings.set(RunSettingsId::RUN_OUTPUT_NAME, file.path.string());
    settings.set(RunSettingsId::RUN_OUTPUT_INTERVAL, 0.01_f);
    settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, 0.03_f);
    settings.set(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP, 10._f);
    settings.set(RunSettingsId::RUN_END_TIME, 0._f);
    SphJob job("HDF5 interval regression", settings);
    AutoPtr<IRun> run = job.getRun(EMPTY_SETTINGS);
    struct Callbacks : IRunCallbacks {
        Float initialStep = 0._f;
        void onSetUp(const Storage&, Statistics& stats) override {
            initialStep = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
        }
        void onTimeStep(const Storage&, Statistics&) override {}
        bool shouldAbortRun() const override {
            return true;
        }
    } callbacks;
    Storage storage = Tests::getGassStorage(20);
    run->run(storage, callbacks);
    REQUIRE(callbacks.initialStep == 0.01_f);
}

TEST_CASE("SPH rejects nonpositive output intervals", "[hdf5][run]") {
    for (const Float interval : { 0._f, -1._f }) {
        RunSettings settings;
        settings.set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::HDF5_FILE);
        settings.set(RunSettingsId::RUN_OUTPUT_INTERVAL, interval);
        SphJob job("Invalid interval", settings);
        REQUIRE_THROWS_AS(job.getRun(EMPTY_SETTINGS), InvalidSetup);
    }
}

TEST_CASE("GADGET HDF5 header and particle IDs", "[hdf5]") {
    Hdf5TestFile output;
    Statistics stats;
    REQUIRE(dumpTestFile(true, output.path, hdf5TestStorage(), stats));
    const hid_t file = H5Fopen(output.path.string().toUtf8(), H5F_ACC_RDONLY, H5P_DEFAULT);
    REQUIRE(file >= 0);
    const hid_t header = H5Gopen2(file, "/Header", H5P_DEFAULT);
    REQUIRE(header >= 0);
    for (const char* name : { "MassTable",
             "BoxSize",
             "NumFilesPerSnapshot",
             "Time",
             "NumPart_ThisFile",
             "NumPart_Total",
             "NumPart_Total_HighWord",
             "NumPart_Total_HW",
             "Redshift",
             "Omega0",
             "OmegaLambda",
             "HubbleParam",
             "Flag_Sfr",
             "Flag_Feedback",
             "Flag_Cooling",
             "Flag_Entropy_ICs",
             "Flag_DoublePrecision" }) {
        REQUIRE(H5Aexists(header, name) > 0);
    }
    uint32_t counts[6] = {};
    const hid_t countsAttr = H5Aopen(header, "NumPart_ThisFile", H5P_DEFAULT);
    REQUIRE(H5Aread(countsAttr, H5T_NATIVE_UINT32, counts) >= 0);
    REQUIRE(H5Aclose(countsAttr) >= 0);
    REQUIRE(counts[0] == 3);
    for (Size i = 1; i < 6; ++i) {
        REQUIRE(counts[i] == 0);
    }
    double masses[6];
    const hid_t massesAttr = H5Aopen(header, "MassTable", H5P_DEFAULT);
    REQUIRE(H5Aread(massesAttr, H5T_NATIVE_DOUBLE, masses) >= 0);
    REQUIRE(H5Aclose(massesAttr) >= 0);
    for (double mass : masses) {
        REQUIRE(mass == 0.);
    }
    int files = 0;
    const hid_t filesAttr = H5Aopen(header, "NumFilesPerSnapshot", H5P_DEFAULT);
    REQUIRE(H5Aread(filesAttr, H5T_NATIVE_INT, &files) >= 0);
    REQUIRE(files == 1);
    REQUIRE(H5Aclose(filesAttr) >= 0);
    uint64_t ids[3] = {};
    const hid_t dataset = H5Dopen2(file, "/PartType0/ParticleIDs", H5P_DEFAULT);
    REQUIRE(dataset >= 0);
    REQUIRE(H5Dread(dataset, H5T_NATIVE_UINT64, H5S_ALL, H5S_ALL, H5P_DEFAULT, ids) >= 0);
    REQUIRE(ids[0] == 1);
    REQUIRE(ids[1] == 2);
    REQUIRE(ids[2] == 3);
    REQUIRE(H5Dclose(dataset) >= 0);
    REQUIRE(H5Gclose(header) >= 0);
    REQUIRE(H5Fclose(file) >= 0);
}

TEST_CASE("HDF5 rejects malformed position shapes", "[hdf5]") {
    for (const std::vector<hsize_t>& shape : { std::vector<hsize_t>{}, { 3 }, { 2, 4 }, { 2, 3, 1 } }) {
        Hdf5TestFile output;
        const hid_t file = createTestFile(output.path);
        Size size = 1;
        for (const hsize_t dim : shape) {
            size *= Size(dim);
        }
        addDataset(file, "/x", shape, std::vector<double>(size));
        REQUIRE(H5Fclose(file) >= 0);
        requireFailedLoad(output.path);
    }
    Hdf5TestFile output;
    const hid_t file = createTestFile(output.path);
    // No allocation needed: a sparse, oversized extent must be rejected before buffer sizing.
    addDataset(file, "/x", { hsize_t(NumericLimits<Size>::max()), 3 }, {});
    REQUIRE(H5Fclose(file) >= 0);
    requireFailedLoad(output.path);
}

TEST_CASE("HDF5 rejects malformed optional datasets and time", "[hdf5]") {
    struct BadDataset {
        const char* name;
        std::vector<hsize_t> dims;
        Size size;
    };
    for (const BadDataset& bad : { BadDataset{ "/m", { 3 }, 3 }, // More rows than coordinates.
             BadDataset{ "/m", { 1 }, 1 },                       // Fewer rows than coordinates.
             BadDataset{ "/m", { 2, 2 }, 4 },
             BadDataset{ "/v", { 2, 4 }, 8 },
             BadDataset{ "/uvw", { 2, 2 }, 4 },
             BadDataset{ "/time", { 2 }, 2 },
             BadDataset{ "/rho", {}, 1 } }) {
        INFO(bad.name);
        Hdf5TestFile output;
        const hid_t file = createTestFile(output.path);
        addDataset(file, "/x", { 2, 3 }, std::vector<double>(6));
        addDataset(file, bad.name, bad.dims, std::vector<double>(bad.size));
        REQUIRE(H5Fclose(file) >= 0);
        requireFailedLoad(output.path);
    }
}

TEST_CASE("HDF5 distinguishes missing data from unreadable data", "[hdf5]") {
    Hdf5TestFile output;
    const hid_t file = createTestFile(output.path);
    addDataset(file, "/x", { 2, 3 }, std::vector<double>(6));
    const hid_t space = H5Screate_simple(1, std::vector<hsize_t>{ 2 }.data(), nullptr);
    REQUIRE(space >= 0);
    const hid_t type = H5Tcopy(H5T_C_S1);
    REQUIRE(type >= 0);
    REQUIRE(H5Tset_size(type, 4) >= 0);
    const hid_t dataset = H5Dcreate2(file, "/m", type, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    REQUIRE(dataset >= 0);
    REQUIRE(H5Dclose(dataset) >= 0);
    REQUIRE(H5Tclose(type) >= 0);
    REQUIRE(H5Sclose(space) >= 0);
    REQUIRE(H5Fclose(file) >= 0);
    requireFailedLoad(output.path);
}

TEST_CASE("HDF5 missing optional fields and column scalar datasets", "[hdf5]") {
    Hdf5TestFile output;
    const hid_t file = createTestFile(output.path);
    addDataset(file, "/x", { 2, 3 }, { 1., 2., 3., 4., 5., 6. });
    addDataset(file, "/m", { 2, 1 }, { 7., 8. });
    REQUIRE(H5Fclose(file) >= 0);
    Storage storage;
    Statistics stats;
    Hdf5Input input;
    REQUIRE(input.load(output.path, storage, stats));
    REQUIRE(storage.isValid());
    REQUIRE(storage.getValue<Float>(QuantityId::MASS)[1] == 8._f);
    REQUIRE(storage.getValue<Float>(QuantityId::DENSITY)[0] == 1000._f);
    REQUIRE(storage.getValue<Float>(QuantityId::PRESSURE)[0] == 0._f);
    REQUIRE(storage.getValue<Vector>(QuantityId::POSITION)[1][H] == 1._f);
    REQUIRE(storage.getDt<Vector>(QuantityId::POSITION)[0] == Vector(0._f));
    REQUIRE_FALSE(storage.has(QuantityId::UVW));
}

TEST_CASE("GADGET loads all six particle groups with MassTable", "[hdf5]") {
    for (const bool allGroups : { false, true }) {
        Hdf5TestFile output;
        const hid_t file = createTestFile(output.path);
        const hid_t header = H5Gcreate2(file, "/Header", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        REQUIRE(header >= 0);
        addAttribute(header, "MassTable", { 0., 10., 0., 0., 40., 0. });
        addAttribute(header, "Time", { 7.5 });
        REQUIRE(H5Gclose(header) >= 0);
        for (Size type = 0; type < 6; ++type) {
            if (!allGroups && type != 4) {
                continue;
            }
            const std::string prefix = "/PartType" + std::to_string(type);
            const hid_t group = H5Gcreate2(file, prefix.c_str(), H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
            REQUIRE(group >= 0);
            addDataset(
                file, prefix + "/Coordinates", { 2, 3 }, { double(type), 0., 0., double(type), 1., 0. });
            if (type != 1 && type != 4) {
                addDataset(file, prefix + "/Masses", { 2 }, { double(type + 1), double(type + 2) });
            }
            if (type == 0) {
                addDataset(file, prefix + "/UVW", { 2, 3 }, std::vector<double>(6, 0.5));
            }
            addDataset(file, prefix + "/SmoothingLengths", { 2 }, { 0.5, 0.75 });
            REQUIRE(H5Gclose(group) >= 0);
        }
        REQUIRE(H5Fclose(file) >= 0);
        Storage storage;
        Statistics stats;
        Hdf5Input input;
        REQUIRE(input.load(output.path, storage, stats));
        REQUIRE(storage.isValid());
        REQUIRE(storage.getParticleCnt() == (allGroups ? 12 : 2));
        REQUIRE(stats.get<Float>(StatisticsId::RUN_TIME) == 7.5_f);
        for (Size i = 0; i < storage.getParticleCnt(); ++i) {
            const Size type = allGroups ? i / 2 : 4;
            REQUIRE(storage.getValue<Size>(QuantityId::FLAG)[i] == type);
            REQUIRE(storage.getValue<Vector>(QuantityId::POSITION)[i][X] == Float(type));
            REQUIRE(storage.getValue<Vector>(QuantityId::POSITION)[i][H] == (i % 2 ? 0.75_f : 0.5_f));
            const Float mass = type == 1 ? 10._f : type == 4 ? 40._f : Float(type + 1 + i % 2);
            REQUIRE(storage.getValue<Float>(QuantityId::MASS)[i] == mass);
            if (allGroups) {
                const Float uv = type == 0 ? 0.5_f : 0._f;
                REQUIRE(storage.getValue<Vector>(QuantityId::UVW)[i] == Vector(uv, uv, uv));
            }
        }
    }
}

TEST_CASE("GADGET rejects missing masses, malformed attributes and split snapshots", "[hdf5]") {
    struct BadHeader {
        const char* name;
        std::vector<double> data;
    };
    for (const BadHeader& bad : { BadHeader{ "MassTable", { 0., 0., 0., 0., 0., 0. } },
             BadHeader{ "MassTable", std::vector<double>(7, 1.) },
             BadHeader{ "Time", { 1., 2. } },
             BadHeader{ "NumFilesPerSnapshot", { 2. } } }) {
        Hdf5TestFile output;
        const hid_t file = createTestFile(output.path);
        const hid_t header = H5Gcreate2(file, "/Header", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        REQUIRE(header >= 0);
        addAttribute(header, bad.name, bad.data);
        REQUIRE(H5Gclose(header) >= 0);
        const hid_t group = H5Gcreate2(file, "/PartType1", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        REQUIRE(group >= 0);
        addDataset(file, "/PartType1/Coordinates", { 1, 3 }, { 0., 0., 0. });
        REQUIRE(H5Gclose(group) >= 0);
        REQUIRE(H5Fclose(file) >= 0);
        requireFailedLoad(output.path);
    }
}

TEST_CASE("HDF5 output reports errors and closes handles", "[hdf5]") {
    Hdf5TestFile output;
    Statistics stats;
    const ssize_t handles = H5Fget_obj_count(H5F_OBJ_ALL, H5F_OBJ_ALL);
    Storage empty;
    REQUIRE_FALSE(dumpTestFile(false, output.path, empty, stats));
    REQUIRE_FALSE(dumpTestFile(true, output.path, empty, stats));
    REQUIRE(H5Fget_obj_count(H5F_OBJ_ALL, H5F_OBJ_ALL) == handles);
    REQUIRE(FileSystem::removePath(output.path));
    REQUIRE(FileSystem::createDirectory(output.path));
    REQUIRE_FALSE(dumpTestFile(false, output.path, hdf5TestStorage(), stats));
    REQUIRE_FALSE(dumpTestFile(true, output.path, hdf5TestStorage(), stats));
    REQUIRE(H5Fget_obj_count(H5F_OBJ_ALL, H5F_OBJ_ALL) == handles);
}

#ifndef SPH_WIN
TEST_CASE("HDF5 output reports filesystem write failures", "[hdf5]") {
    for (const bool gadget : { false, true }) {
        Hdf5TestFile output;
        const pid_t child = fork();
        REQUIRE(child >= 0);
        if (child == 0) {
            // Isolate the file-size limit and HDF5's failed-close state from the test runner.
            std::signal(SIGXFSZ, SIG_IGN);
            struct rlimit limit;
            if (getrlimit(RLIMIT_FSIZE, &limit) != 0) {
                _exit(2);
            }
            limit.rlim_cur = 4096;
            if (setrlimit(RLIMIT_FSIZE, &limit) != 0) {
                _exit(2);
            }
            Statistics stats;
            const bool success = bool(dumpTestFile(gadget, output.path, hdf5TestStorage(8192), stats));
            _exit(success ? 1 : 0);
        }
        int status = 0;
        REQUIRE(waitpid(child, &status, 0) == child);
        REQUIRE(WIFEXITED(status));
        REQUIRE(WEXITSTATUS(status) == 0);
    }
}
#endif

#endif // SPH_USE_HDF5
