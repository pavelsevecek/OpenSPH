TEMPLATE = app
CONFIG += c++14 thread
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -Wall -Wextra -Werror -msse4.1 -std=c++14 -pthread

DEPENDPATH += . ../../lib
INCLUDEPATH += ../../lib ../../../external/openvdb
PRE_TARGETDEPS += ../../lib/liblib.a

LIBS += ../../lib/liblib.a /home/pavel/projects/astro/sph/external/openvdb/openvdb/libopenvdb.a

# -llog4cplus
QMAKE_LFLAGS += -ltbb -lIlmImf -lHalf -lblosc -lboost_iostreams -lboost_system -lboost_thread -lz -L/home/pavel/projects/astro/sph/external/openvdb/openvdb/libopenvdb.a
#LIBS += /usr/lib/libtbb.so
#LIBS += ../../../external/openvdb/openvdb/libopenvdb.so


CONFIG(release, debug|profile|assert|release) {
  message( "SPH Launcher --- Building for Release" )
  QMAKE_CXXFLAGS += -O4
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH Launcher --- Building for Profile" )
  DEFINES += SPH_PROFILE
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH Launcher --- Building for Assert" )
  DEFINES += SPH_DEBUG SPH_PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH Launcher --- Building for Debug" )
  DEFINES += SPH_DEBUG SPH_PROFILE
}

SOURCES += \
    SsfToVdb.cpp
