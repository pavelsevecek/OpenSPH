TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

DEPENDPATH += . ../../core
PRE_TARGETDEPS += ../../core/libcore.a
LIBS += ../../core/libcore.a

include(../../core/sharedCore.pro)

INCLUDEPATH += ../../core $$PREFIX/include/catch2

SOURCES += \
    main.cpp \
    sod/Sod.cpp \
    sod/Solution.cpp \
    sedov/Sedov.cpp \
    wave/SoundWave.cpp \
    elastic_bands/ElasticBands.cpp \
    cliff_collapse/CliffCollapse.cpp \
    collision/Collision.cpp \
    galaxy/Galaxy.cpp \
    kelvin_helmholtz/KelvinHelmholtz.cpp \
    boundary/Boundary.cpp \
    rotating_rod/RotatingRod.cpp \
    Common.cpp

HEADERS += \   \
    sod/Solution.h
    Common.h
