TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += ../lib ..
DEPENDPATH += ../lib
LIBS += ../lib/liblib.a
PRE_TARGETDEPS += ../lib/liblib.a

include(../lib/inc.pro)

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
