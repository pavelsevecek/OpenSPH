#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include "io/Logger.h"
#include "utils/Utils.h"

int main(int argc, char* argv[]) {
    Sph::Assert::isTest = true;
    int result = Catch::Session().run(argc, argv);
    if (!skippedTests.empty()) {
        Sph::Console console;
        console.fg = Sph::Console::Foreground::LIGHT_YELLOW;
        console.series = Sph::Console::Series::BOLD;
        Sph::ScopedConsole sc(console);
        std::cout << "\033[F" << console << "Skipped " << skippedTests.size() << " tests" << std::endl
                  << std::endl;
    }
    return (result < 0xff ? result : 0xff);
}
