TEMPLATE = app
CONFIG += c++14 thread
CONFIG -= app_bundle
CONFIG -= qt


DEPENDPATH += . ../../lib
INCLUDEPATH += ../../lib ../../../external/openvdb
PRE_TARGETDEPS += ../../lib/liblib.a

LIBS += ../../lib/liblib.a /home/pavel/projects/astro/sph/external/openvdb/openvdb/libopenvdb.a

# -llog4cplus
QMAKE_LFLAGS += -ltbb -lIlmImf -lHalf -lblosc -lboost_iostreams -lboost_system -lboost_thread -lz -L/home/pavel/projects/astro/sph/external/openvdb/openvdb/libopenvdb.a

include(../../lib/sharedLib.pro)

SOURCES += \
    SsfToVdb.cpp
