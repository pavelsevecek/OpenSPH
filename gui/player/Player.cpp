#include "gui/player/Player.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "io/FileSystem.h"
#include "io/Logger.h"
#include "objects/utility/StringUtils.h"
#include <wx/msgdlg.h>

IMPLEMENT_APP(Sph::App);

NAMESPACE_SPH_BEGIN

RunPlayer::RunPlayer() {}

void RunPlayer::setUp() {
    if (wxTheApp->argc == 1) {
        wxMessageBox("Specify file mask as a parameter", "Error", wxOK);
        return;
    }

    std::string arg(wxTheApp->argv[1]);
    // we also allow passing the first file instead of the mask
    arg = replaceAll(arg, "0000", "%d");
    files = OutputFile(Path(arg));
    fileCnt = this->getFileCount(Path(arg));

    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    const Path firstPath = files.getNextPath(stats);
    if (!FileSystem::pathExists(firstPath)) {
        executeOnMainThread(
            [firstPath] { wxMessageBox("Cannot locate file " + firstPath.native(), "Error", wxOK); });
        return;
    }

    storage = makeShared<Storage>();
    BinaryOutput io;
    Outcome result = io.load(firstPath, *storage, stats);
    if (!result) {
        executeOnMainThread([firstPath] {
            wxMessageBox("Cannot load the run state file " + firstPath.native(), "Error", wxOK);
        });
        return;
    } else {
        // const Float t0 = stats.get<Float>(StatisticsId::RUN_TIME);
        const Float dt = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);
        // const Interval origRange = settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE);
        // settings.set(RunSettingsId::RUN_TIME_RANGE, Interval(t0, origRange.upper()));
        settings.set(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP, dt);
    }

    callbacks = makeAuto<GuiCallbacks>(*controller);
}

Size RunPlayer::getFileCount(const Path& pathMask) const {
    OutputFile of(pathMask);
    Size cnt = 0;
    Statistics stats;
    while (true) {
        Path path = of.getNextPath(stats);
        if (FileSystem::pathExists(path)) {
            ++cnt;
        } else {
            break;
        }
    }
    return cnt;
}

void RunPlayer::run() {
    ASSERT(storage);
    setNullToDefaults();
    logger->write("Running:");

    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    callbacks->onRunStart(*storage, stats);

    const Size stepMilliseconds = Size(1000._f / fps);
    for (Size i = 0; i < fileCnt; ++i) {
        Timer stepTimer;
        stats.set(StatisticsId::RELATIVE_PROGRESS, Float(i) / fileCnt);
        callbacks->onTimeStep(*storage, stats);

        BinaryOutput io;
        const Path path = files.getNextPath(stats);
        storage = makeShared<Storage>();
        const Outcome result = io.load(path, *storage, stats);
        if (!result) {
            executeOnMainThread(
                [path] { wxMessageBox("Cannot load the run state file " + path.native(), "Error", wxOK); });
            break;
        }

        const Size elapsed = stepTimer.elapsed(TimerUnit::MILLISECOND);
        if (elapsed < stepMilliseconds) {
            std::this_thread::sleep_for(std::chrono::milliseconds(stepMilliseconds - elapsed));
        }

        // triggers
        /*for (auto iter = triggers.begin(); iter != triggers.end();) {
            ITrigger& trig = **iter;
            if (trig.condition(*storage, stats)) {
                AutoPtr<ITrigger> newTrigger = trig.action(*storage, stats);
                if (newTrigger) {
                    triggers.pushBack(std::move(newTrigger));
                }
                if (trig.type() == TriggerEnum::ONE_TIME) {
                    iter = triggers.erase(iter);
                    continue;
                }
            }
            ++iter;
        }*/

        if (callbacks->shouldAbortRun()) {
            break;
        }
    }
    callbacks->onRunEnd(*storage, stats);
    this->tearDownInternal();
}

void RunPlayer::tearDown() {}

NAMESPACE_SPH_END
