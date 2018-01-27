#include "thread/CheckFunction.h"
#include "common/Assert.h"
#include <thread>

NAMESPACE_SPH_BEGIN

const std::thread::id MAIN_THREAD_ID = std::this_thread::get_id();

FunctionChecker::FunctionChecker(std::atomic<Size>& reentrantCnt, std::atomic<Size>& totalCnt)
    : reentrantCnt(reentrantCnt)
    , totalCnt(totalCnt) {
    reentrantCnt++;
    totalCnt++;
}

FunctionChecker::~FunctionChecker() {
    MARK_USED(totalCnt); // to silence clang warning
    reentrantCnt--;
}

void FunctionChecker::check(const Flags<CheckFunction>& flags) {
    if (flags.has(CheckFunction::MAIN_THREAD)) {
        ASSERT(std::this_thread::get_id() == MAIN_THREAD_ID, "Called from different thread");
    }
    if (flags.has(CheckFunction::NOT_MAIN_THREAD)) {
        ASSERT(std::this_thread::get_id() != MAIN_THREAD_ID, "Called from main thread");
    }
    if (flags.has(CheckFunction::NON_REENRANT)) {
        ASSERT(reentrantCnt == 1, "Reentrant " + std::to_string(reentrantCnt));
    }
    if (flags.has(CheckFunction::ONCE)) {
        ASSERT(totalCnt == 1, "Called more than once");
    }
}

bool isMainThread() {
    return std::this_thread::get_id() == MAIN_THREAD_ID;
}

NAMESPACE_SPH_END
