#include "common/Assert.h"
#include "io/Logger.h"
#include "objects/containers/Array.h"
#include "system/Platform.h"
#include <assert.h>
#include <mutex>
#ifndef SPH_MSVC
#include <execinfo.h>
#include <signal.h>
#endif

NAMESPACE_SPH_BEGIN

static Array<std::string> getStackTrace() {
#ifdef SPH_MSVC
    return {};
#else
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
#endif
}

static void breakToDebugger() {
#ifdef SPH_MSVC
    if (isDebuggerPresent()) {
        __debugbreak();
    }
#else
    if (isDebuggerPresent()) {
        raise(SIGTRAP);
    }
#endif
}

bool Assert::throwAssertException = false;

bool Assert::isTest = false;

Assert::Handler Assert::handler = nullptr;

void Assert::fireParams(const char* message,
    const char* file,
    const char* func,
    const int line,
    const char* text) {
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    if (!throwAssertException) {
        AutoPtr<ILogger> logger;
        if (handler) {
            // write the message to string and provide it to the custom handler
            logger = makeAuto<StringLogger>();
        } else {
            // by default, print the message to stdout
            logger = makeAuto<StdOutLogger>();

            // also add some padding
            logger->write(
                "============================================================================================"
                "==============");
        }

        logger->write("Assert fired in file ", file, ", executing function ", func, " on line ", line);
        logger->write("Condition: ", message);
        if (strlen(text) != 0) {
            logger->write("Assert parameters: ", text);
        }
        if (false) {
            logger->write("Stack trace:");
            Array<std::string> trace = getStackTrace();
            for (std::string& s : trace) {
                logger->write(s);
            }
        }

        if (handler) {
            // execute the custom assert handler
            const bool retval = (*handler)(dynamic_cast<StringLogger*>(&*logger)->toString());
            if (!retval) {
                // ignore the assert
                return;
            }
        } else {
            logger->write(
                "============================================================================================"
                "==============");
        }

        breakToDebugger();

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
    breakToDebugger();
}

NAMESPACE_SPH_END
