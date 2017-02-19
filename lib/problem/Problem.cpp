#include "problem/Problem.h"
#include "physics/Integrals.h"
#include "solvers/AbstractSolver.h"
#include "solvers/SolverFactory.h"
#include "sph/timestepping/TimeStepping.h"
#include "system/Callbacks.h"
#include "system/Factory.h"
#include "system/Logger.h"
#include "system/Output.h"
#include "system/Statistics.h"
#include "system/Timer.h"

NAMESPACE_SPH_BEGIN

Problem::Problem(const GlobalSettings& settings, const std::shared_ptr<Storage> storage)
    : settings(settings)
    , storage(storage) {
    solver = getSolver(settings);
    logger = Factory::getLogger(settings);
}

Problem::~Problem() = default;

struct EndingCondition {
private:
    Float wallclockDuraction;
    Size timestepCnt;

public:
    EndingCondition(const Float wallclockDuraction, const Size timestepCnt)
        : wallclockDuraction(wallclockDuraction)
        , timestepCnt(timestepCnt) {}

    bool operator()(const Timer& timer, const Size timestep) {
        if (wallclockDuraction > 0._f && timer.elapsed<TimerUnit::MILLISECOND>() > wallclockDuraction) {
            return true;
        }
        if (timestepCnt > 0 && timestep >= timestepCnt) {
            return true;
        }
        return false;
    }
};

void Problem::run() {
    Size i = 0;
    // fetch parameters of run from settings
    const Float outputInterval = settings.get<Float>(GlobalSettingsIds::RUN_OUTPUT_INTERVAL);
    const Range timeRange = settings.get<Range>(GlobalSettingsIds::RUN_TIME_RANGE);
    // construct timestepping object
    timeStepping = Factory::getTimeStepping(settings, storage);

    // run main loop
    Float nextOutput = outputInterval;
    logger->write("Running:");
    Timer runTimer;
    EndingCondition condition(settings.get<Float>(GlobalSettingsIds::RUN_WALLCLOCK_TIME),
        settings.get<int>(GlobalSettingsIds::RUN_TIMESTEP_CNT));
    Statistics stats;
    // ArrayView<Float> energy = storage->getValue<Float>(QuantityIds::ENERGY);
    FileLogger energyLogger("totalEnergy.txt");
    FileLogger stressLogger("stress.txt");
    TotalEnergy en;
    TotalKineticEnergy kinEn;
    TotalInternalEnergy intEn;

    QuantityExpression sumStress([](Storage& storage) {
        ArrayView<const TracelessTensor> s =
            storage.getValue<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS);
        ArrayView<const Size> flag = storage.getValue<Size>(QuantityIds::FLAG);
        Float avg = 0._f;
        Size cnt = 0;
        for (Size i = 0; i < storage.getParticleCnt(); ++i) {
            if (flag[i] == 1) {
                avg += sqrt(ddot(s[i], s[i]));
                cnt++;
            }
        }
        ASSERT(cnt != 0);
        return avg / cnt;
    });
    QuantityExpression sumDtStress([](Storage& storage) {
        ArrayView<const TracelessTensor> ds = storage.getDt<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS);
        ArrayView<const Size> flag = storage.getValue<Size>(QuantityIds::FLAG);
        Float avg = 0._f;
        Size cnt = 0;
        for (Size i = 0; i < storage.getParticleCnt(); ++i) {
            if (flag[i] == 1) {
                avg += sqrt(ddot(ds[i], ds[i]));
                cnt++;
            }
        }
        ASSERT(cnt != 0);
        return avg / cnt;
    });

    QuantityMeans avgPressure(QuantityIds::PRESSURE, 1);
    QuantityMeans avgEnergy(QuantityIds::ENERGY, 1);
    QuantityMeans avgDensity(QuantityIds::DENSITY, 1);


    QuantityExpression sumGradv([](Storage& storage) {
        ArrayView<const Tensor> gradv = storage.getValue<Tensor>(QuantityIds::RHO_GRAD_V);
        ArrayView<const Float> rho = storage.getValue<Float>(QuantityIds::DENSITY);
        ArrayView<const Size> flag = storage.getValue<Size>(QuantityIds::FLAG);
        Float avg = 0._f;
        Size cnt = 0;
        for (Size i = 0; i < storage.getParticleCnt(); ++i) {
            if (flag[i] == 1) {
                avg += sqrt(ddot(gradv[i], gradv[i])) / rho[i];
                cnt++;
            }
        }
        ASSERT(cnt != 0);
        return avg / cnt;
    });
    for (Float t = timeRange.lower(); t < timeRange.upper() && !condition(runTimer, i);
         t += timeStepping->getTimeStep()) {
        if (callbacks) {
            callbacks->onTimeStep((t - timeRange.lower()) / timeRange.size(), storage);
            if (callbacks->shouldAbortRun()) {
                break;
            }
        }

        // dump output
        if (output && t >= nextOutput) {
            output->dump(*storage, t);
            nextOutput += outputInterval;
        }

        // make time step
        timeStepping->step(*solver, stats);

        // log
        stats.set(StatisticsIds::TIME, t);
        stats.set(StatisticsIds::INDEX, (int)i);
        FrequentStatsFormat format;
        format.print(*logger, stats);

        energyLogger.write(t,
            "   ",
            en.evaluate(*storage),
            "   ",
            kinEn.evaluate(*storage),
            "   ",
            intEn.evaluate(*storage));

        stressLogger.write(t,
            "   ",
            sumStress.evaluate(*storage),
            "   ",
            sumDtStress.evaluate(*storage),
            "   ",
            sumGradv.evaluate(*storage),
            "   ",
            avgEnergy.evaluate(*storage).average(),
            "   ",
            avgPressure.evaluate(*storage).average(),
            "  ",
            avgDensity.evaluate(*storage).average());
        i++;
    }
    logger->write("Run ended after ", runTimer.elapsed<TimerUnit::SECOND>(), "s.");
}

NAMESPACE_SPH_END
