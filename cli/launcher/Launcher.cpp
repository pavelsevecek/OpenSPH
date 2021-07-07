/// \brief Executable running a simulation previously set up by GUI appliaction

#include "Sph.h"
#include "run/Config.h"
#include "run/Node.h"
#include "run/SpecialEntries.h"
#include "run/jobs/GeometryJobs.h"
#include "run/jobs/InitialConditionJobs.h"
#include "run/jobs/IoJobs.h"
#include "run/jobs/MaterialJobs.h"
#include "run/jobs/ParticleJobs.h"
#include "run/jobs/SimulationJobs.h"

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
            case IVirtualEntry::Type::INTERVAL:
                entry.set(input.get<Interval>(name));
                break;
            case IVirtualEntry::Type::VECTOR:
                entry.set(input.get<Vector>(name));
                break;
            case IVirtualEntry::Type::STRING:
                entry.set(input.get<std::string>(name));
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
        } catch (const Exception& e) {
            logger.write("Failed to load value, keeping the default.\n", e.what());
        }
    }
};

/// \todo AVOID THIS
static void registerJobs() {
    static SphJob sSph("");
    static CollisionGeometrySetup sSetup("");
    static MonolithicBodyIc sIc("");
    static SaveFileJob sIo("");
    static BlockJob sBlock("");
    static MaterialJob sMat("");
}

static void run(const ArgParser& parser, ILogger& logger) {
#ifdef SPH_VERSION
    logger.write("Running opensph-cli (version ", SPH_STR(SPH_VERSION), ")");
#else
    logger.write("Running opensph-cli (unknown version)");
#endif
    const Path projectPath(parser.getArg<std::string>("p"));
    const std::string nodeToRun(parser.getArg<std::string>("n"));

    Config config;
    config.load(projectPath);

    FlatMap<std::string, SharedPtr<JobNode>> nodes;

    SharedPtr<ConfigNode> inNodes = config.getNode("nodes");
    // lists node connections: node, target slot and target node
    Array<Tuple<SharedPtr<JobNode>, std::string, std::string>> allToConnect;

    inNodes->enumerateChildren([&nodes, &allToConnect, &logger](std::string name, ConfigNode& input) {
        const std::string className = input.get<std::string>("class_name");
        RawPtr<IJobDesc> desc = getJobDesc(className);
        if (!desc) {
            throw Exception("Cannot find desc for node '" + className + "'");
        }

        AutoPtr<IJob> worker = desc->create(name);
        SharedPtr<JobNode> node = makeShared<JobNode>(std::move(worker));
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

    Optional<SharedPtr<JobNode>&> runner = nodes.tryGet(nodeToRun);
    if (!runner) {
        throw Exception("No node '" + nodeToRun + "' found in the project");
    }

    logger.write("Running node tree:");
    runner.value()->enumerate([&logger](SharedPtr<JobNode> node, const Size depth) {
        logger.write(std::string(depth * 3, ' '), " - ", node->instanceName());
    });

    /// \todo properly load globals
    RunSettings globals = EMPTY_SETTINGS;
    globals.set(RunSettingsId::RUN_THREAD_CNT, 0);
    globals.set(RunSettingsId::RUN_THREAD_GRANULARITY, 1000);
    globals.set(RunSettingsId::RUN_RNG, RngEnum::UNIFORM);
    globals.set(RunSettingsId::RUN_RNG_SEED, 1234);
    globals.set(RunSettingsId::SPH_KERNEL, KernelEnum::CUBIC_SPLINE);
    globals.set(RunSettingsId::GENERATE_UVWS, false);
    NullJobCallbacks callbacks;
    runner.value()->run(globals, callbacks);
}

int main(int argc, char* argv[]) {
    StdOutLogger logger;
    registerJobs();

    try {
        ArgParser parser(params);
        parser.parse(argc, argv);

        run(parser, logger);

        return 0;

    } catch (HelpException& e) {
        printBanner(logger);
        logger.write(e.what());
        return 0;
    } catch (const Exception& e) {
        logger.write("Run failed!\n", e.what());
        return -1;
    }
}
