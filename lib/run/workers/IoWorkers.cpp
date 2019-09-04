#include "run/workers/IoWorkers.h"
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
// LoadFileWorker
// ----------------------------------------------------------------------------------------------------------

VirtualSettings LoadFileWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& cat = connector.addCategory("Input");
    cat.connect("File", "file", path);
    return connector;
}

void LoadFileWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    AutoPtr<IInput> input = Factory::getInput(path);
    Storage storage;
    Statistics stats;
    Outcome outcome = input->load(Path(path), storage, stats);
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

static WorkerRegistrar sRegisterLoadFile("load file", "I/O", [](const std::string& UNUSED(name)) {
    return makeAuto<LoadFileWorker>();
});

// ----------------------------------------------------------------------------------------------------------
// FileSequenceWorker
// ----------------------------------------------------------------------------------------------------------

VirtualSettings FileSequenceWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& inputCat = connector.addCategory("Input");
    inputCat.connect("First file", "first_file", firstFile);
    inputCat.connect("Maximum framerate", "max_fps", maxFps);

    VirtualSettings::Category& cacheCat = connector.addCategory("Cache");
    cacheCat.connect("Cache loaded file", "do_caching", cache.use)
        .setTooltip(
            "If true, loaded files are kept in memory, allowing to run the sequence much faster in the "
            "following evaluations.\n\nCurrently only particle positions, velocities and radii are cached in "
            "order to reduce the memory of loaded files.");

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
    ASSERT(firstIndex); // already checked above

    const Path dir = firstFile.parentPath();
    for (Path relativePath : FileSystem::iterateDirectory(dir)) {
        const Path path = dir / relativePath;
        Optional<OutputFile> pathMask = OutputFile::getMaskFromPath(path);
        if (pathMask && pathMask->getMask() == referenceMask->getMask()) {
            // belongs to the same file sequence
            Optional<Size> index = OutputFile::getDumpIdx(path);
            ASSERT(index);

            if (index.value() >= firstIndex.value()) {
                fileMap.insert(index.value(), path);
            }
        }
    }
    return fileMap;
}


void FileSequenceWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& callbacks) {
    AutoPtr<IInput> input = Factory::getInput(firstFile);
    Storage storage;
    Statistics stats;

    FlatMap<Size, Path> sequence = getFileSequence(firstFile);
    const Size firstIndex = sequence.begin()->key;
    const Size lastIndex = (sequence.end() - 1)->key;
    for (auto& element : sequence) {
        const Size index = element.key;

        Timer frameTimer;
        if (cache.use && cache.data.contains(index)) {
            storage = Storage(cache.data[index]);
        } else {
            Outcome outcome = input->load(element.value, storage, stats);
            if (!outcome) {
                throw InvalidSetup(outcome.error());
            }

            if (cache.use) {
                cache.data.insert(index, CompressedStorage(storage));
            }
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

static WorkerRegistrar sRegisterFileSequence("load sequence", "sequence", "I/O", [](const std::string& name) {
    return makeAuto<FileSequenceWorker>(name);
});

// ----------------------------------------------------------------------------------------------------------
// SaveFileWorker
// ----------------------------------------------------------------------------------------------------------

SaveFileWorker::SaveFileWorker(const std::string& name)
    : IParticleWorker(name) {
    settings.set(RunSettingsId::RUN_OUTPUT_TYPE, IoEnum::BINARY_FILE)
        .set(RunSettingsId::RUN_OUTPUT_PATH, std::string(""))
        .set(RunSettingsId::RUN_OUTPUT_NAME, std::string("final.ssf"))
        .set(RunSettingsId::RUN_OUTPUT_QUANTITIES,
            OutputQuantityFlag::POSITION | OutputQuantityFlag::VELOCITY);
}

VirtualSettings SaveFileWorker::getSettings() {
    VirtualSettings connector;

    VirtualSettings::Category& outputCat = connector.addCategory("Output");
    outputCat.connect<Path>("File", settings, RunSettingsId::RUN_OUTPUT_NAME);
    outputCat.connect<EnumWrapper>("Format", settings, RunSettingsId::RUN_OUTPUT_TYPE);
    outputCat.connect<Flags<OutputQuantityFlag>>("Quantities", settings, RunSettingsId::RUN_OUTPUT_QUANTITIES)
        .setEnabler([this] {
            const IoEnum type = settings.get<IoEnum>(RunSettingsId::RUN_OUTPUT_TYPE);
            return type == IoEnum::TEXT_FILE || type == IoEnum::VTK_FILE;
        });

    return connector;
}

void SaveFileWorker::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& UNUSED(callbacks)) {
    SharedPtr<ParticleData> data = this->getInput<ParticleData>("particles");

    AutoPtr<IOutput> output = Factory::getOutput(settings);
    output->dump(data->storage, data->stats);

    result = data;
}

static WorkerRegistrar sRegisterOutput("save file", "I/O", [](const std::string& name) {
    return makeAuto<SaveFileWorker>(name);
});

// ----------------------------------------------------------------------------------------------------------
// SaveMeshWorker
// ----------------------------------------------------------------------------------------------------------

VirtualSettings SaveMeshWorker::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& outputCat = connector.addCategory("Output");
    outputCat.connect("File", "file", path);

    VirtualSettings::Category& meshCat = connector.addCategory("Mesh parameters");
    meshCat.connect<Float>("Resolution", "resolution", resolution);
    meshCat.connect<Float>("Surface level", "level", level);
    meshCat.connect<bool>("Scale to unit size", "scale_to_unit", scaleToUnit);

    return connector;
}

void SaveMeshWorker::evaluate(const RunSettings& global, IRunCallbacks& callbacks) {
    SharedPtr<ParticleData> data = this->getInput<ParticleData>("particles");

    // sanitize resolution
    Box bbox;
    ArrayView<const Vector> r = data->storage.getValue<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        bbox.extend(r[i]);
    }

    const Float boxSize = maxElement(bbox.size());
    Float step = clamp(resolution, 1.e-3_f * boxSize, 0.2_f * boxSize);

    SharedPtr<IScheduler> scheduler = Factory::getScheduler(global);

    auto callback = [&callbacks](const Float progress) {
        Statistics stats;
        stats.set(StatisticsId::RELATIVE_PROGRESS, progress);
        callbacks.onTimeStep(Storage(), stats);
        return !callbacks.shouldAbortRun();
    };
    Array<Triangle> triangles = getSurfaceMesh(*scheduler, data->storage, step, level, callback);

    if (scaleToUnit) {
        for (Triangle& t : triangles) {
            for (Size i = 0; i < 3; ++i) {
                t[i] = (t[i] - bbox.center()) / boxSize;
            }
        }
    }

    AutoPtr<IMeshFile> saver = getMeshFile(path);
    Outcome outcome = saver->save(path, std::move(triangles));
    if (!outcome) {
        throw InvalidSetup("Saving mesh failed.\n\n" + outcome.error());
    }

    result = data;
}

static WorkerRegistrar sRegisterMeshSaver("save mesh", "I/O", [](const std::string& name) {
    return makeAuto<SaveMeshWorker>(name);
});

NAMESPACE_SPH_END
