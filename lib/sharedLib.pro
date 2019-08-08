QMAKE_CXXFLAGS += -Wall -Wextra -msse4.1 -std=c++14 -pthread

linux-g++ {
    # seems like gcc reports false positive for this warning
    QMAKE_CXXFLAGS += -Wno-aggressive-loop-optimizations
}


CONFIG(use_tbb) {
    DEFINES += SPH_USE_TBB
    INCLUDEPATH += /usr/include/tbb
    LIBS += -ltbb -ltbb_debug -ltbbmalloc
}

CONFIG(use_eigen) {
    DEFINES += SPH_USE_EIGEN
    INCLUDEPATH += /usr/include/eigen3
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
