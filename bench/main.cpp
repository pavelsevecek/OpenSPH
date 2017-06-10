#include "bench/Session.h"

int main(int argc, char* argv[]) {
    Sph::Assert::isTest = true;
    Sph::Benchmark::Session::getInstance().run(argc, argv);
    return 0;
}
