#pragma once

#include "io/Output.h"
#include "quantities/Storage.h"
#include "system/Statistics.h"
#include <csignal>

NAMESPACE_SPH_BEGIN

class CrashPad {
private:
    SharedPtr<Storage> storage;
    Path dumpPath;

public:
    static void setup(SharedPtr<Storage> storage, const Path& dumpPath) {
        getInstance().storage = storage;
        getInstance().dumpPath = dumpPath;
        signal(SIGSEGV, handler);
    }

    static CrashPad& getInstance() {
        static CrashPad instance;
        return instance;
    }

private:
    static void handler(const int UNUSED(signal)) {
        /*void* array[10];
        size_t size;

        // get void*'s for all entries on the stack
        size = backtrace(array, 10);

        // print out all the frames to stderr
        fprintf(stderr, "Error: signal %d:\n", sig);
        backtrace_symbols_fd(array, size, STDERR_FILENO);*/
        BinaryOutput output(getInstance().dumpPath);
        Statistics stats;
        stats.set(StatisticsId::RUN_TIME, 0._f);
        stats.set(StatisticsId::TIMESTEP_VALUE, 1._f);
        output.dump(*getInstance().storage, stats);
        std::exit(1);
    }
};

NAMESPACE_SPH_END
