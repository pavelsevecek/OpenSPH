#include "run/jobs/IoJobs.h"
#include "io/FileSystem.h"
#include "io/Output.h"
#include "objects/geometry/Delaunay.h"
#include "post/MarchingCubes.h"
#include "post/MeshFile.h"
#include "run/IRun.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "system/Timer.h"
#include <thread>

NAMESPACE_SPH_BEGIN

// ----------------------------------------------------------------------------------------------------------
// LoadFileJob
// ----------------------------------------------------------------------------------------------------------

static RegisterEnum<UnitEnum> sUnits({
    { UnitEnum::SI, "SI", "SI unit system" },
    { UnitEnum::CGS, "CGS", "CGS unit system" },
    { UnitEnum::NBODY, "nbody", "N-body (Hénon) units" },
});

VirtualSettings LoadFileJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& cat = connector.addCategory("Input");
    cat.connect("File", "file", path)
        .setPathType(IVirtualEntry::PathType::INPUT_FILE)
        .setFileFormats(getInputFormats());
    cat.connect("Unit system", "units", units);
    return connector;
}

void LoadFileJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    if (!FileSystem::pathExists(path)) {
        throw InvalidSetup("File '" + path.native() + "' does not exist or cannot be accessed.");
    }
    AutoPtr<IInput> input = Factory::getInput(path);
    Storage storage;
    Statistics stats;
    Outcome outcome = input->load(path, storage, stats);
    if (!outcome) {
        throw InvalidSetup(outcome.error());
    }

    result = makeShared<ParticleData>();

    // set up overrides for resuming simulations
    if (stats.has(StatisticsId::RUN_TIME)) {
        result->overrides.set(RunSettingsId::RUN_START_TIME, stats.get<Float>(StatisticsId::RUN_TIME));
    }
    if (stats.has(StatisticsId::TIMESTEP_VALUE)) {
        result->overrides.set(
            RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, stats.get<Float>(StatisticsId::TIMESTEP_VALUE));
    }
    if (Optional<Size> dumpIdx = OutputFile::getDumpIdx(Path(path))) {
        result->overrides.set(RunSettingsId::RUN_OUTPUT_FIRST_INDEX, int(dumpIdx.value()));
    }

    Float G;
    switch (UnitEnum(units)) {
    case UnitEnum::SI:
        G = Constants::gravity;
        break;
    case UnitEnum::CGS:
        G = 1.e3_f * Constants::gravity;
        break;
    case UnitEnum::NBODY:
        G = 1._f;
        break;
    default:
        NOT_IMPLEMENTED;
    }

    result->overrides.set(RunSettingsId::GRAVITY_CONSTANT, G);

    result->storage = std::move(storage);
    result->stats = std::move(stats);
}

static JobRegistrar sRegisterLoadFile(
    "load file",
    "I/O",
    [](const std::string& UNUSED(name)) { return makeAuto<LoadFileJob>(); },
    "Loads particle state from a file");

// ----------------------------------------------------------------------------------------------------------
// FileSequenceJob
// ----------------------------------------------------------------------------------------------------------

VirtualSettings FileSequenceJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& inputCat = connector.addCategory("Input");
    inputCat.connect("First file", "first_file", firstFile)
        .setPathType(IVirtualEntry::PathType::INPUT_FILE)
        .setFileFormats(getInputFormats());
    inputCat.connect("Maximum framerate", "max_fps", maxFps);

    return connector;
}

/// \todo deduplicate with timeline
FlatMap<Size, Path> getFileSequence(const Path& firstFile) {
    if (!FileSystem::pathExists(firstFile)) {
        throw InvalidSetup("File '" + firstFile.native() + "' does not exist.");
    }

    FlatMap<Size, Path> fileMap;
    Optional<OutputFile> referenceMask = OutputFile::getMaskFromPath(firstFile);
    if (!referenceMask) {
        throw InvalidSetup("Cannot deduce sequence from file '" + firstFile.native() + "'.");
    }

    Optional<Size> firstIndex = OutputFile::getDumpIdx(firstFile);
    SPH_ASSERT(firstIndex); // already checked above

    const Path dir = firstFile.parentPath();
    for (Path relativePath : FileSystem::iterateDirectory(dir)) {
        const Path path = dir / relativePath;
        Optional<OutputFile> pathMask = OutputFile::getMaskFromPath(path);
        if (pathMask && pathMask->getMask() == referenceMask->getMask()) {
            // belongs to the same file sequence
            Optional<Size> index = OutputFile::getDumpIdx(path);
            SPH_ASSERT(index);

            if (index.value() >= firstIndex.value()) {
                fileMap.insert(index.value(), path);
            }
        }
    }
    return fileMap;
}


void FileSequenceJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& callbacks) {
    AutoPtr<IInput> input = Factory::getInput(firstFile);
    Storage storage;
    Statistics stats;

    FlatMap<Size, Path> sequence = getFileSequence(firstFile);
    const Size firstIndex = sequence.begin()->key();
    const Size lastIndex = (sequence.end() - 1)->key();
    for (auto& element : sequence) {
        const Size index = element.key();

        Timer frameTimer;
        Outcome outcome = input->load(element.value(), storage, stats);
        if (!outcome) {
            throw InvalidSetup(outcome.error());
        }

        stats.set(StatisticsId::INDEX, int(index));
        if (sequence.size() > 1) {
            stats.set(StatisticsId::RELATIVE_PROGRESS, Float(index - firstIndex) / (lastIndex - firstIndex));
        } else {
            stats.set(StatisticsId::RELATIVE_PROGRESS, 1._f);
        }

        if (index == firstIndex) {
            callbacks.onSetUp(storage, stats);
        }
        callbacks.onTimeStep(storage, stats);

        if (callbacks.shouldAbortRun()) {
            break;
        }

        const Size elapsed = frameTimer.elapsed(TimerUnit::MILLISECOND);
        const Size minElapsed = 1000 / maxFps;
        if (elapsed < minElapsed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(minElapsed - elapsed));
        }
    }

    result = makeShared<ParticleData>();
    result->storage = std::move(storage);
    result->stats = std::move(stats);
}

static JobRegistrar sRegisterFileSequence(
    "load sequence",
    "sequence",
    "I/O",
    [](const std::string& name) { return makeAuto<FileSequenceJob>(name); },
    "Loads and displays a sequence of particle states.");

// ----------------------------------------------------------------------------------------------------------
// SaveFileJob
// ----------------------------------------------------------------------------------------------------------

SaveFileJob::SaveFileJob(const std::string& name)
    : IParticleJob(name) {
    settings.set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::BINARY_FILE)
        .set(RunSettingsId::RUN_OUTPUT_PATH, std::string(""))
        .set(RunSettingsId::RUN_OUTPUT_NAME, std::string("final.ssf"))
        .set(RunSettingsId::RUN_OUTPUT_QUANTITIES,
            OutputQuantityFlag::POSITION | OutputQuantityFlag::VELOCITY);
}

VirtualSettings SaveFileJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& outputCat = connector.addCategory("Output");
    outputCat.connect<Path>("File", settings, RunSettingsId::RUN_OUTPUT_NAME)
        .setPathType(IVirtualEntry::PathType::OUTPUT_FILE)
        .setFileFormats(getOutputFormats());
    outputCat.connect<EnumWrapper>("Format", settings, RunSettingsId::RUN_OUTPUT_TYPE);
    outputCat.connect<Flags<OutputQuantityFlag>>("Quantities", settings, RunSettingsId::RUN_OUTPUT_QUANTITIES)
        .setEnabler([this] {
            const IoEnum type = settings.get<IoEnum>(RunSettingsId::RUN_OUTPUT_TYPE);
            return type == IoEnum::TEXT_FILE || type == IoEnum::VTK_FILE;
        });

    return connector;
}

void SaveFileJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<ParticleData> data = this->getInput<ParticleData>("particles");

    AutoPtr<IOutput> output = Factory::getOutput(settings);
    output->dump(data->storage, data->stats);

    result = data;
}

static JobRegistrar sRegisterOutput(
    "save file",
    "I/O",
    [](const std::string& name) { return makeAuto<SaveFileJob>(name); },
    "Saves the input particle state into a file.");

// ----------------------------------------------------------------------------------------------------------
// SaveMeshJob
// ----------------------------------------------------------------------------------------------------------

static RegisterEnum<MeshAlgorithm> sMeshAlgorithm({
    { MeshAlgorithm::MARCHING_CUBES,
        "marching_cubes",
        "Isosurface extracted using the Marching Cubes algorithm." },
    { MeshAlgorithm::ALPHA_SHAPE, "alpha_shape", "Alpha shape obtained from Delaunay triangulation." },
});

static Float getMedianRadius(ArrayView<const Vector> r) {
    Array<Float> h(r.size());
    for (Size i = 0; i < r.size(); ++i) {
        h[i] = r[i][H];
    }
    std::nth_element(h.begin(), h.begin() + h.size() / 2, h.end());
    return h[h.size() / 2];
}

VirtualSettings SaveMeshJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& outputCat = connector.addCategory("Output");
    outputCat.connect("File", "file", path)
        .setPathType(IVirtualEntry::PathType::OUTPUT_FILE)
        .setFileFormats({
            { "Wavefront OBJ file", "obj" },
            { "Stanford PLY file", "ply" },
        });

    auto mcEnabler = [this] { return MeshAlgorithm(algorithm) == MeshAlgorithm::MARCHING_CUBES; };
    auto alphaEnabler = [this] { return MeshAlgorithm(algorithm) == MeshAlgorithm::ALPHA_SHAPE; };

    VirtualSettings::Category& meshCat = connector.addCategory("Mesh parameters");
    meshCat.connect("Algorithm", "algorithm", algorithm);
    meshCat.connect("Resolution", "resolution", resolution).setEnabler(mcEnabler);
    meshCat.connect("Surface level", "level", level).setEnabler(mcEnabler);
    meshCat.connect("Anisotropic kernels", "aniso", anisotropic).setEnabler(mcEnabler);
    meshCat.connect("Smoothing multiplier", "smoothing_mult", smoothingMult).setEnabler(mcEnabler);
    meshCat.connect("Alpha value", "alpha", alpha).setEnabler(alphaEnabler);
    meshCat.connect("Refine mesh", "refine", refine);
    meshCat.connect("Scale to unit size", "scale_to_unit", scaleToUnit);

    return connector;
}

void SaveMeshJob::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    SharedPtr<ParticleData> data = this->getInput<ParticleData>("particles");

    Array<Triangle> triangles;
    switch (MeshAlgorithm(algorithm)) {
    case MeshAlgorithm::MARCHING_CUBES:
        triangles = runMarchingCubes(data->storage, global, callbacks);
        break;
    case MeshAlgorithm::ALPHA_SHAPE:
        triangles = runAlphaShape(data->storage, callbacks);
        break;
    default:
        NOT_IMPLEMENTED;
    }

    if (scaleToUnit) {
        const Box bbox = getBoundingBox(data->storage);
        const Float boxSize = maxElement(bbox.size());
        for (Triangle& t : triangles) {
            for (Size i = 0; i < 3; ++i) {
                t[i] = (t[i] - bbox.center()) / boxSize;
            }
        }
    }

    if (refine) {
        Mesh mesh = getMeshFromTriangles(triangles, 1.e-6_f);
        for (Size i = 0; i < 5; ++i) {
            refineMesh(mesh);
        }
        triangles = getTrianglesFromMesh(mesh);
    }

    AutoPtr<IMeshFile> saver = getMeshFile(path);
    Outcome outcome = saver->save(path, std::move(triangles));
    if (!outcome) {
        throw InvalidSetup("Saving mesh failed.\n\n" + outcome.error());
    }

    result = data;
}

Array<Triangle> SaveMeshJob::runMarchingCubes(const Storage& storage,
    const RunSettings& global,
    IRunCallbacks& callbacks) const {
    McConfig config;
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    config.gridResolution = resolution * getMedianRadius(r);
    config.surfaceLevel = level;
    config.smoothingMult = smoothingMult;
    config.useAnisotropicKernels = anisotropic;
    config.progressCallback = RunCallbacksProgressibleAdapter(callbacks);
    SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);
    return getSurfaceMesh(*scheduler, storage, config);
}

Array<Triangle> SaveMeshJob::runAlphaShape(const Storage& storage, IRunCallbacks& callbacks) const {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    Delaunay delaunay;
    delaunay.setProgressCallback(RunCallbacksProgressibleAdapter(callbacks));
    delaunay.build(r);

    return delaunay.alphaShape(alpha * getMedianRadius(r));
}

static JobRegistrar sRegisterMeshSaver(
    "save mesh",
    "I/O",
    [](const std::string& name) { return makeAuto<SaveMeshJob>(name); },
    "Creates a triangular mesh from the input particles and saves it to file.");

NAMESPACE_SPH_END
