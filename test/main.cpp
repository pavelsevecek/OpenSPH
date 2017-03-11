#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include "core/Assert.h"

int main(int argc, char* argv[]) {
    Sph::Assert::isTest = true;
    int result = Catch::Session().run(argc, argv);
    return (result < 0xff ? result : 0xff);
}
