#include "run/jobs/ScriptJobs.h"
#include "quantities/Quantity.h"
#include "run/IRun.h"
#include "run/ScriptUtils.h"
#include "system/Factory.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

#ifdef SPH_USE_CHAISCRIPT

ChaiScriptJob::ChaiScriptJob(const String& name)
    : IParticleJob(name) {
    slotNames.resize(8);
    for (Size i = 0; i < slotNames.size(); ++i) {
        slotNames[i] = "slot " + toString(i + 1);
    }

    paramNames.resize(8);
    paramValues.resize(8);
    for (Size i = 0; i < paramNames.size(); ++i) {
        paramNames[i] = "parameter " + toString(i + 1);
        paramValues[i] = 0._f;
    }
}

UnorderedMap<String, ExtJobType> ChaiScriptJob::getSlots() const {
    UnorderedMap<String, ExtJobType> slots;
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
        inputCat.connect("Slot " + toString(i + 1), "slot" + toString(i + 1), slotNames[i]);
    }
    inputCat.connect("Number of parameters", "paramCnt", paramCnt);
    for (int i = 0; i < paramCnt; ++i) {
        inputCat.connect("Parameter " + toString(i + 1), "param" + toString(i + 1), paramNames[i]);
        inputCat.connect("Value '" + paramNames[i] + "'", "value" + toString(i + 1), paramValues[i]);
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
    chai.add(chaiscript::fun<std::function<Chai::Particles(std::string)>>([this](std::string nameUtf) {
        String name = String::fromUtf8(nameUtf.c_str());
        SharedPtr<ParticleData> input = this->getInput<ParticleData>(name);
        Chai::Particles particles;
        particles.bindToStorage(input->storage);
        return particles;
    }),
        "getInput");
    chai.add(chaiscript::fun<std::function<Float(std::string)>>([this](std::string nameUtf) {
        String name = String::fromUtf8(nameUtf.c_str());
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

    /// \todo use short path on Windows?
    chaiscript::Boxed_Value scriptValue = chai.eval_file(std::string(file.string().toUtf8()));
    const Chai::Particles& particles = chaiscript::boxed_cast<const Chai::Particles&>(scriptValue);
    result = makeShared<ParticleData>();
    result->storage = particles.store().clone(VisitorEnum::ALL_BUFFERS);
}

static JobRegistrar sRegisterChaiJob(
    "custom script",
    "particle operators",
    [](const String& name) { return makeAuto<ChaiScriptJob>(name); },
    "Custom particle operator, given by a ChaiScript file.");

#endif

NAMESPACE_SPH_END
