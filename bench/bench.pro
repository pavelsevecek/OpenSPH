TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -Wall -Werror -msse4.1 -std=c++14 -pthread

INCLUDEPATH += ../lib ..
DEPENDPATH += ../lib
LIBS += ../lib/liblib.a
PRE_TARGETDEPS += ../lib/liblib.a


CONFIG(release, debug|profile|assert|release) {
  message( "SPH BENCHMARK --- Building for Release" )
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH BENCHMARK --- Building for Profile" )
  DEFINES += SPH_PROFILE
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH BENCHMARK --- Building for Assert" )
  DEFINES += SPH_DEBUG SPH_PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH BENCHMARK --- Building for Debug" )
  DEFINES += SPH_DEBUG SPH_PROFILE
}

SOURCES += main.cpp \
    Session.cpp \
    examples/TestBenchmark.cpp \
    ../lib/objects/finders/benchmark/Finders.cpp \
    ../lib/sph/kernel/benchmark/Kernel.cpp \
    ../lib/gravity/benchmark/Gravity.cpp \
    ../lib/objects/containers/benchmark/Map.cpp \
    ../lib/sph/solvers/benchmark/Solvers.cpp

HEADERS += \
    Session.h \
    Stats.h \
    Common.h
