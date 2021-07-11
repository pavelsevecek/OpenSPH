#include "run/jobs/ScriptJobs.h"
#include "quantities/Quantity.h"
#include "run/IRun.h"
#include "run/ScriptUtils.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

#ifdef SPH_USE_CHAISCRIPT

ChaiScriptJob::ChaiScriptJob(const std::string& name)
    : IParticleJob(name) {
    slotNames.resize(8);
    for (Size i = 0; i < slotNames.size(); ++i) {
        slotNames[i] = "slot " + std::to_string(i + 1);
    }

    paramNames.resize(8);
    paramValues.resize(8);
    for (Size i = 0; i < paramNames.size(); ++i) {
        paramNames[i] = "parameter " + std::to_string(i + 1);
        paramValues[i] = 0._f;
    }
}

UnorderedMap<std::string, ExtJobType> ChaiScriptJob::getSlots() const {
    UnorderedMap<std::string, ExtJobType> slots;
    for (int i = 0; i < min<int>(inputCnt, slotNames.maxSize()); ++i) {
        slots.insert(slotNames[i], JobType::PARTICLES);
    }
    return slots;
}


VirtualSettings ChaiScriptJob::getSettings() {
    VirtualSettings connector;
    addGenericCategory(connector, instName);

    VirtualSettings::Category& inputCat = connector.addCategory("Input");
    inputCat.connect("Number of inputs", "inputCnt", inputCnt);
    for (int i = 0; i < inputCnt; ++i) {
        inputCat.connect("Slot " + std::to_string(i + 1), "slot" + std::to_string(i + 1), slotNames[i]);
    }
    inputCat.connect("Number of parameters", "paramCnt", paramCnt);
    for (int i = 0; i < paramCnt; ++i) {
        inputCat.connect(
            "Parameter " + std::to_string(i + 1), "param" + std::to_string(i + 1), paramNames[i]);
        inputCat.connect("Value '" + paramNames[i] + "'", "value" + std::to_string(i + 1), paramValues[i]);
    }

    VirtualSettings::Category& scriptCat = connector.addCategory("Script");
    scriptCat.connect<Path>("Script file", "file", file)
        .setPathType(IVirtualEntry::PathType::INPUT_FILE)
        .setFileFormats({ { "Chaiscript script", "chai" } });

    return connector;
}

void ChaiScriptJob::evaluate(const RunSettings& UNUSED(global), IRunCallbacks& callbacks) {
    chaiscript::ChaiScript chai;
    Chai::registerBindings(chai);

    // node-specific stuff
    /*for (int i = 0; i < inputCnt; ++i) {
        SharedPtr<ParticleData> input = this->getInput<ParticleData>(slotNames[i]);
        chai.add(chaiscript::var(std::ref(input->storage)), slotNames[i]);
    }*/
    chai.add(chaiscript::fun<std::function<Chai::Particles(std::string)>>([this](std::string name) {
        SharedPtr<ParticleData> input = this->getInput<ParticleData>(name);
        Chai::Particles particles;
        particles.bindToStorage(input->storage);
        return particles;
    }),
        "getInput");
    chai.add(chaiscript::fun<std::function<Float(std::string)>>([this](std::string name) {
        for (Size i = 0; i < paramNames.size(); ++i) {
            if (name == paramNames[i]) {
                return paramValues[i];
            }
        }
        throw InvalidSetup("Unknown parameter '" + name + "'");
    }),
        "getParam");
    Statistics stats;
    stats.set(StatisticsId::RELATIVE_PROGRESS, 0._f);
    chai.add(chaiscript::fun<std::function<void(Float)>>([&callbacks, &stats](const Float progress) {
        stats.set(StatisticsId::RELATIVE_PROGRESS, progress);
        callbacks.onTimeStep({}, stats);
    }),
        "setProgress");
    chai.add(chaiscript::fun<std::function<bool()>>([&callbacks]() { return callbacks.shouldAbortRun(); }),
        "shouldAbort");


    chaiscript::Boxed_Value scriptValue = chai.eval_file(file.native());
    const Chai::Particles& particles = chaiscript::boxed_cast<const Chai::Particles&>(scriptValue);
    result = makeShared<ParticleData>();
    result->storage = particles.store().clone(VisitorEnum::ALL_BUFFERS);
}

static JobRegistrar sRegisterChaiWorker(
    "custom script",
    "particle operators",
    [](const std::string& name) { return makeAuto<ChaiScriptJob>(name); },
    "Custom particle operator, given by a ChaiScript file.");

#endif

NAMESPACE_SPH_END
