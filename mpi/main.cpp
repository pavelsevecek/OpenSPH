#include "Mpi.h"
#include <iostream>

using namespace Sph;

int main() {
    Mpi& mpi = Mpi::getInstance();
    std::cout << mpi.getProcessorName() << ", rank: " << mpi.getProcessRank() << std::endl;
    Mpi::shutdown();
    return 0;
}
