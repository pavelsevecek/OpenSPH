#include "common/Assert.h"
#include "io/Logger.h"
#include "system/Platform.h"
#include <assert.h>
#include <mutex>
#include <signal.h>

NAMESPACE_SPH_BEGIN

bool Assert::isTest = false;
bool Assert::breakOnFail = true;

void Assert::check(const bool condition,
    const char* message,
    const char* file,
    const char* func,
    const int line,
    const char* text) {
    if (condition) {
        return;
    }
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    if (breakOnFail) {
        StdOutLogger logger;
        logger.write(
            "=============================================================================================");
        logger.write("Assert fired in file ", file, ", executing function ", func, " on line ", line);
        logger.write("Condition: ", message);
        if (strlen(text) != 0) {
            logger.write("Assert parameters: ", text);
        }
        logger.write(
            "=============================================================================================");
        if (isDebuggerPresent()) {
            raise(SIGTRAP);
        }
    }
    if (!isTest) {
        assert(condition);
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
