#include "bench/Session.h"

/// \section Benchmarking
///
/// Benchmark can run

int main(int argc, char* argv[]) {
    Sph::Benchmark::Session::getInstance().run(argc, argv);
    return 0;
}
