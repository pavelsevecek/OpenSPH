#include "run/jobs/IoJobs.h"
#include "io/FileSystem.h"
#include "io/Output.h"
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

VirtualSettings LoadFileJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& cat = connector.addCategory("Input");
    cat.connect("File", "file", path)
        .setPathType(IVirtualEntry::PathType::INPUT_FILE)
        .setFileFormats(getInputFormats());
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
    result->storage = std::move(storage);
    result->stats = std::move(stats);

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
    const Size firstIndex = sequence.begin()->key;
    const Size lastIndex = (sequence.end() - 1)->key;
    for (auto& element : sequence) {
        const Size index = element.key;

        Timer frameTimer;
        Outcome outcome = input->load(element.value, storage, stats);
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

VirtualSettings SaveMeshJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& outputCat = connector.addCategory("Output");
    outputCat.connect("File", "file", path)
        .setPathType(IVirtualEntry::PathType::INPUT_FILE)
        .setFileFormats({
            { "Wavefront OBJ file", "obj" },
            { "Stanford PLY file", "ply" },
        });

    VirtualSettings::Category& meshCat = connector.addCategory("Mesh parameters");
    meshCat.connect<Float>("Resolution", "resolution", resolution);
    meshCat.connect<Float>("Surface level", "level", level);
    meshCat.connect<Float>("Smoothing multiplier", "smoothing_mult", smoothingMult);
    meshCat.connect<bool>("Refine mesh", "refine", refine);
    meshCat.connect<bool>("Anisotropic kernels", "aniso", anisotropic);
    meshCat.connect<bool>("Scale to unit size", "scale_to_unit", scaleToUnit);

    return connector;
}

void SaveMeshJob::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    SharedPtr<ParticleData> data = this->getInput<ParticleData>("particles");

    // sanitize resolution
    const Box bbox = getBoundingBox(data->storage);
    const Float boxSize = maxElement(bbox.size());

    SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);

    McConfig config;
    config.gridResolution = clamp(resolution, 1.e-3_f * boxSize, 0.2_f * boxSize);
    config.surfaceLevel = level;
    config.smoothingMult = smoothingMult;
    config.useAnisotropicKernels = anisotropic;
    config.progressCallback = [&callbacks](const Float progress) {
        Statistics stats;
        stats.set(StatisticsId::RELATIVE_PROGRESS, progress);
        callbacks.onTimeStep(Storage(), stats);
        return !callbacks.shouldAbortRun();
    };
    Array<Triangle> triangles = getSurfaceMesh(*scheduler, data->storage, config);

    if (scaleToUnit) {
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

static JobRegistrar sRegisterMeshSaver(
    "save mesh",
    "I/O",
    [](const std::string& name) { return makeAuto<SaveMeshJob>(name); },
    "Creates a triangular mesh from the input particles and saves it to file.");

NAMESPACE_SPH_END
