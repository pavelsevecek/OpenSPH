TEMPLATE = app
CONFIG += c++14 thread
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -msse4.1 -std=c++14 -pthread -pg
QMAKE_LFLAGS += -pg

INCLUDEPATH += ../lib ../../external/Catch/include
DEPENDPATH += . ../lib
LIBS += ../lib/libsph.a

SOURCES += \
    main.cpp \
    utils/Utils.cpp \
    ../lib/math/test/Integrator.cpp \
    ../lib/math/test/Math.cpp \
    ../lib/math/rng/test/Rng.cpp \
    ../lib/math/rng/test/VectorRng.cpp \
    ../lib/physics/test/TimeFormat.cpp \
    ../lib/physics/test/Eos.cpp \
    ../lib/geometry/test/Vector.cpp \
    ../lib/geometry/test/Indices.cpp \
    ../lib/objects/wrappers/test/Optional.cpp \
    ../lib/objects/wrappers/test/Variant.cpp \
    ../lib/objects/wrappers/test/Iterators.cpp \
    ../lib/objects/wrappers/test/Range.cpp \
    ../lib/objects/wrappers/test/Shadow.cpp \
    ../lib/objects/wrappers/test/NonOwningPtr.cpp \
    ../lib/objects/wrappers/test/Any.cpp \
    ../lib/objects/wrappers/test/VectorizedArray.cpp \
    ../lib/objects/containers/test/Array.cpp \
    ../lib/objects/containers/test/ArrayUtils.cpp \
    ../lib/objects/finders/test/KdTree.cpp \
    ../lib/objects/finders/test/LinkedList.cpp \
    ../lib/objects/finders/test/Order.cpp \
    ../lib/objects/finders/test/BruteForce.cpp \
    ../lib/sph/timestepping/test/TimeStepping.cpp \
    ../lib/sph/distributions/test/Distribution.cpp \
    ../lib/sph/kernel/test/Kernel.cpp \
    ../lib/system/test/Settings.cpp \
    ../lib/system/test/Timer.cpp \
    ../lib/storage/test/Quantity.cpp \
    ../lib/storage/test/Storage.cpp \
    ../lib/objects/containers/test/BufferedArray.cpp \
    ../lib/objects/containers/test/Tuple.cpp \
    ../lib/objects/wrappers/test/Flags.cpp \
    ../lib/physics/test/Integrals.cpp \
    ../lib/geometry/test/Tensor.cpp \
    ../lib/system/test/Profiler.cpp \
    ../lib/objects/containers/test/LimitedArray.cpp \
    ../lib/storage/test/Iterate.cpp \
    ../lib/storage/test/QuantityHelpers.cpp \
    ../lib/geometry/test/TracelessTensor.cpp \
    ../lib/system/test/Output.cpp \
    ../lib/solvers/test/ContinuitySolver.cpp


HEADERS += \
    utils/Utils.h
