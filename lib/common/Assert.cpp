#include "core/Assert.h"

NAMESPACE_SPH_BEGIN

bool Assert::isTest = false;

void Assert::check(const bool condition, const char* message) {
    if (!isTest) {
        assert(condition);
    } else if (!condition) {
        throw Exception(message);
    }
}

NAMESPACE_SPH_END
