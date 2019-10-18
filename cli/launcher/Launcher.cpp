/// \brief Executable running a simulation previously set up by GUI appliaction

#include "Sph.h"
#include "run/Config.h"
#include "run/Node.h"
#include "run/SpecialEntries.h"
#include "run/workers/InitialConditionWorkers.h"
#include "run/workers/IoWorkers.h"
#include "run/workers/ParticleWorkers.h"
#include "run/workers/SimulationWorkers.h"

using namespace Sph;

static Array<ArgDesc> params{
    { "p", "project", ArgEnum::STRING, "Path to the project file." },
    { "n", "node", ArgEnum::STRING, "Name of the node to evaluate" },
};

static void printBanner(ILogger& logger) {
    logger.write("*******************************************************************************");
    logger.write("******************************** OpenSPH CLI **********************************");
    logger.write("*******************************************************************************");
}

/// \todo deduplicate
class LoadProc : public VirtualSettings::IEntryProc {
private:
    ConfigNode& input;
    ILogger& logger;

public:
    LoadProc(ConfigNode& input, ILogger& logger)
        : input(input)
        , logger(logger) {}

    virtual void onCategory(const std::string& UNUSED(name)) const override {}

    virtual void onEntry(const std::string& name, IVirtualEntry& entry) const override {
        const IVirtualEntry::Type type = entry.getType();

        try {
            switch (type) {
            case IVirtualEntry::Type::BOOL:
                entry.set(input.get<bool>(name));
                break;
            case IVirtualEntry::Type::INT:
                entry.set(input.get<int>(name));
                break;
            case IVirtualEntry::Type::FLOAT:
                entry.set(input.get<Float>(name));
                break;
            case IVirtualEntry::Type::VECTOR:
                entry.set(input.get<Vector>(name));
                break;
            case IVirtualEntry::Type::STRING:
                break;
            case IVirtualEntry::Type::PATH:
                entry.set(input.get<Path>(name));
                break;
            case IVirtualEntry::Type::ENUM:
            case IVirtualEntry::Type::FLAGS: {
                EnumWrapper ew = entry.get();
                ew.value = input.get<int>(name);
                entry.set(ew);
                break;
            }
            case IVirtualEntry::Type::EXTRA: {
                /// \todo currently used only by curves, can be generalized if needed
                ExtraEntry extra(makeAuto<CurveEntry>());
                extra.fromString(input.get<std::string>(name));
                entry.set(extra);
                break;
            }
            default:
                NOT_IMPLEMENTED;
            }
        } catch (Exception& e) {
            logger.write("Failed to load value, keeping the default.\n", e.what());
        }
    }
};

/// \todo AVOID THIS
static void registerRunners() {
    static SphWorker sSph("");
    static CollisionGeometrySetup sSetup("");
    static MonolithicBodyIc sIc("");
    static SaveFileWorker sIo("");
}

static void run(const ArgParser& parser, ILogger& logger) {
    const Path projectPath(parser.getArg<std::string>("p"));
    const std::string nodeToRun(parser.getArg<std::string>("n"));

    Config config;
    config.load(projectPath);

    /*SharedPtr<ConfigNode> inGlobals = config.getNode("globals");
    VirtualSettings globalSettings = this->getGlobalSettings();
    globalSettings.enumerate(LoadProc(*inGlobals));*/
    FlatMap<std::string, SharedPtr<WorkerNode>> nodes;

    SharedPtr<ConfigNode> inNodes = config.getNode("nodes");
    // lists node connections: node, target slot and target node
    Array<Tuple<SharedPtr<WorkerNode>, std::string, std::string>> allToConnect;

    inNodes->enumerateChildren([&nodes, &allToConnect, &logger](std::string name, ConfigNode& input) {
        const std::string className = input.get<std::string>("class_name");
        RawPtr<IWorkerDesc> desc = getWorkerDesc(className);
        if (!desc) {
            throw Exception("Cannot find desc for node '" + className + "'");
        }

        AutoPtr<IWorker> worker = desc->create(name);
        SharedPtr<WorkerNode> node = makeShared<WorkerNode>(std::move(worker));
        nodes.insert(name, node);

        VirtualSettings settings = node->getSettings();
        settings.enumerate(LoadProc(input, logger));

        for (Size i = 0; i < node->getSlotCnt(); ++i) {
            const std::string slotName = node->getSlot(i).name;
            Optional<std::string> connectedName = input.tryGet<std::string>(slotName);
            if (connectedName) {
                allToConnect.push(makeTuple(node, slotName, connectedName.value()));
            }
        }
    });

    for (auto& toConnect : allToConnect) {
        for (auto& element : nodes) {
            if (element.key == toConnect.get<2>()) {
                element.value->connect(toConnect.get<0>(), toConnect.get<1>());
            }
        }
    }

    Optional<SharedPtr<WorkerNode>&> runner = nodes.tryGet(nodeToRun);
    if (!runner) {
        throw Exception("No node '" + nodeToRun + "' found in the project");
    }

    logger.write("Running node tree:");
    runner.value()->enumerate([&logger](SharedPtr<WorkerNode> node, const Size depth) {
        logger.write(std::string(depth * 3, ' '), " - ", node->instanceName());
    });

    RunSettings globals = EMPTY_SETTINGS; /// \todo
    NullWorkerCallbacks callbacks;
    runner.value()->run(globals, callbacks);
}

int main(int argc, char* argv[]) {
    StdOutLogger logger;
    registerRunners();

    try {
        ArgParser parser(params);
        parser.parse(argc, argv);

        run(parser, logger);

        return 0;

    } catch (HelpException& e) {
        printBanner(logger);
        logger.write(e.what());
        return 0;
    } catch (Exception& e) {
        logger.write("Run failed!\n", e.what());
        return -1;
    }
}
