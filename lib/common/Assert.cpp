#include "common/Assert.h"
#include "io/Logger.h"
#include "objects/containers/Array.h"
#include "system/Platform.h"
#include <assert.h>
#include <execinfo.h>
#include <mutex>
#include <signal.h>

NAMESPACE_SPH_BEGIN

static Array<std::string> getStackTrace() {
    constexpr Size TRACE_SIZE = 10;
    void* buffer[TRACE_SIZE];
    const Size nptrs = backtrace(buffer, TRACE_SIZE);

    char** strings = backtrace_symbols(buffer, nptrs);
    if (!strings) {
        return { "No backtrace could be generated!" };
    }

    Array<std::string> trace;
    for (Size i = 0; i < nptrs; ++i) {
        trace.push(strings[i]);
    }
    free(strings);
    return trace;
}

bool Assert::isTest = false;
bool Assert::breakOnFail = true;

void Assert::fireParams(const char* message,
    const char* file,
    const char* func,
    const int line,
    const char* text) {
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    if (breakOnFail) {
        StdOutLogger logger;
        logger.write(
            "================================================================================================"
            "==========");
        logger.write("Assert fired in file ", file, ", executing function ", func, " on line ", line);
        logger.write("Condition: ", message);
        if (strlen(text) != 0) {
            logger.write("Assert parameters: ", text);
        }
        logger.write("Stack trace:");
        Array<std::string> trace = getStackTrace();
        for (std::string& s : trace) {
            logger.write(s);
        }
        logger.write(
            "================================================================================================"
            "==========");
        if (isDebuggerPresent()) {
            raise(SIGTRAP);
        }
    }
    if (!isTest) {
        assert(false);
    } else {
        throw Exception(message);
    }
}

void Assert::todo(const char* message, const char* func, const int line) {
    StdOutLogger logger;
    logger.write("===========================================================");
    logger.write("Missing implementation at ", func, " on line ", line);
    logger.write(message);
    logger.write("===========================================================");
    if (isDebuggerPresent()) {
        raise(SIGTRAP);
    }
}

NAMESPACE_SPH_END
