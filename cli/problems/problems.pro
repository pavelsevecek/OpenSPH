TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -msse4.1 -Wall -Werror -std=c++14 -pthread


INCLUDEPATH += ../../lib ../../../external/Catch/include
DEPENDPATH += . ../../lib
PRE_TARGETDEPS += ../../lib/liblib.a
LIBS += ../../lib/liblib.a


CONFIG(release, debug|profile|assert|release) {
  message( "SPH PROBLEMS --- Building for Release" )
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH PROBLEMS --- Building for Profile" )
  DEFINES += SPH_PROFILE
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH PROBLEMS --- Building for Assert" )
  DEFINES += SPH_DEBUG SPH_PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH PROBLEMS --- Building for Debug" )
  DEFINES += SPH_DEBUG SPH_PROFILE
}

SOURCES += \
    main.cpp \
    sod/Sod.cpp \
    wave/SoundWave.cpp

HEADERS += \  
    sod/solution.h
