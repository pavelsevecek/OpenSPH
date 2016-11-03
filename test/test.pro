TEMPLATE = app
CONFIG += c++14 thread
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -msse4.1 -std=c++14 -pthread

INCLUDEPATH += ../src ../external/Catch/include
DEPENDPATH += . ../src
LIBS += ../src/libsph.a

SOURCES += \
    main.cpp \
    utils/Utils.cpp \
    ../src/math/test/Integrator.cpp \
    ../src/math/test/Math.cpp \
    ../src/math/rng/test/Rng.cpp \
    ../src/math/rng/test/VectorRng.cpp \
    ../src/physics/test/TimeFormat.cpp \
    ../src/physics/test/Eos.cpp \
    ../src/geometry/test/Vector.cpp \
    ../src/geometry/test/Indices.cpp \
    ../src/objects/wrappers/test/Optional.cpp \
    ../src/objects/wrappers/test/Variant.cpp \
    ../src/objects/wrappers/test/Iterators.cpp \
    ../src/objects/wrappers/test/Range.cpp \
    ../src/objects/wrappers/test/Shadow.cpp \
    ../src/objects/wrappers/test/NonOwningPtr.cpp \
    ../src/objects/wrappers/test/Any.cpp \
    ../src/objects/containers/test/Array.cpp \
    ../src/objects/containers/test/ArrayUtils.cpp \
    ../src/objects/finders/test/KdTree.cpp \
    ../src/objects/finders/test/LinkedList.cpp \
    ../src/objects/finders/test/Order.cpp \
    ../src/objects/finders/test/BruteForce.cpp \
    ../src/sph/initconds/test/InitConds.cpp \
    ../src/sph/kernel/test/Kernel.cpp \
    ../src/system/test/Settings.cpp \
    ../src/system/test/Timer.cpp

HEADERS += \
    utils/Utils.h
