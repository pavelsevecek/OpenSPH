#include "common/Assert.h"
#include "io/Logger.h"
#include "objects/containers/Array.h"
#include "system/Platform.h"
#include <assert.h>
#include <mutex>
#include <signal.h>

#ifndef SPH_WIN
#include <execinfo.h>
#endif

NAMESPACE_SPH_BEGIN

bool Assert::throwAssertException = false;

bool Assert::isTest = false;

Assert::Handler Assert::handler = nullptr;

#ifdef SPH_WIN
#define SPH_DEBUG_BREAK __debugbreak()
#else
#define SPH_DEBUG_BREAK raise(SIGTRAP)
#endif

void Assert::fireParams(const char* message,
    const char* file,
    const char* func,
    const int line,
    const char* text) {
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);

    AutoPtr<ILogger> logger;
    if (handler) {
        // write the message to string and provide it to the custom handler
        logger = makeAuto<StringLogger>();
    } else {
        // by default, print the message to stdout
#ifdef SPH_WIN
        logger = makeAuto<ConsoleLogger>();
#else
        logger = makeAuto<StdOutLogger>();
#endif

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

    if (!throwAssertException) {
        if (isDebuggerPresent()) {
            SPH_DEBUG_BREAK;
        }

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
        SPH_DEBUG_BREAK;
    }
}

NAMESPACE_SPH_END
