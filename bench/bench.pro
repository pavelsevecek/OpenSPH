TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

INCLUDEPATH += ../core ..
DEPENDPATH += ../core
LIBS += ../core/libcore.a
PRE_TARGETDEPS += ../core/libcore.a

include(../core/sharedCore.pro)

SOURCES += main.cpp \
    Session.cpp \
    ../core/objects/finders/benchmark/Finders.cpp \
    ../core/sph/kernel/benchmark/Kernel.cpp \
    ../core/gravity/benchmark/Gravity.cpp \
    ../core/objects/containers/benchmark/Map.cpp \
    ../core/sph/solvers/benchmark/Solvers.cpp

HEADERS += \
    Session.h \
    Stats.h \
    Common.h
