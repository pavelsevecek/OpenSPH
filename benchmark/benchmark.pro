TEMPLATE = app
CONFIG += console c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -Wall -Werror -msse4.1 -std=c++1z -pthread
QMAKE_CXX = g++


CONFIG(release, debug|profile|assert|release) {
  message( "SPH BENCHMARK --- Building for Release" )
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH BENCHMARK --- Building for Profile" )
  DEFINES += PROFILE
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH BENCHMARK --- Building for Assert" )
  DEFINES += DEBUG PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH BENCHMARK --- Building for Debug" )
  DEFINES += DEBUG PROFILE
}

INCLUDEPATH += ../lib ../../external/benchmark/include
DEPENDPATH += . ../lib
LIBS += ../lib/libsph.a -lbenchmark
SOURCES += main.cpp \
    ../lib/objects/finders/benchmark/Finders.cpp \
    ../lib/solvers/benchmark/ContinuitySolver.cpp
