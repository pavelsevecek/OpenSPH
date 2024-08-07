QMAKE_CXXFLAGS += -Wall -Wextra -msse4.1 -std=c++14 -pthread

linux-g++ {
    # extra warnings
    QMAKE_CXXFLAGS += -Wfloat-conversion -Wduplicated-cond -Wduplicated-branches -Wlogical-op

    # seems like gcc reports false positive for this warning
    QMAKE_CXXFLAGS += -Wno-aggressive-loop-optimizations

    # necessary for backward compatibility (gcc-7 would fail to compile)
    QMAKE_CXXFLAGS += -Wno-redundant-move
}

isEmpty(PREFIX) {
    PREFIX = /usr
}

CONFIG(use_tbb) {
    DEFINES += SPH_USE_TBB TBB_SUPPRESS_DEPRECATED_MESSAGES
    INCLUDEPATH += $$PREFIX/include/tbb
    LIBS += -ltbb -ltbbmalloc
}

CONFIG(use_openmp) {
    DEFINES += SPH_USE_OPENMP
    QMAKE_CXXFLAGS += -fopenmp
    QMAKE_LFLAGS += -fopenmp
}

CONFIG(use_eigen) {
    DEFINES += SPH_USE_EIGEN
    INCLUDEPATH += $$PREFIX/include/eigen3
}

CONFIG(use_hdf5) {
    DEFINES += SPH_USE_HDF5
    LIBS += -lhdf5
}

CONFIG(use_chaiscript) {
    DEFINES += SPH_USE_CHAISCRIPT
    LIBS += -ldl
}

CONFIG(use_vdb) {
    DEFINES += SPH_USE_VDB
    LIBS += -ltbb -lImath -lopenvdb
}

CONFIG(static_libc) {
    QMAKE_LFLAGS += -static -static-libstdc++ -static-libgcc -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
}

CONFIG(devel) {
    QMAKE_CXXFLAGS += -Werror
}

CONFIG(profile_build) {
    QMAKE_CXX = time g++
}

CONFIG(release, debug|profile|assert|release) {
  message( "SPH --- Building for Release" )
  QMAKE_CXXFLAGS += -O2 -ffast-math
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH --- Building for Profile" )
  DEFINES += SPH_PROFILE
  QMAKE_CXXFLAGS += -O2 -ffast-math
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH --- Building for Assert" )
  DEFINES += SPH_DEBUG
  QMAKE_CXXFLAGS += -O2 -ffast-math
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH --- Building for Debug" )
  DEFINES += SPH_DEBUG
}
