TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CXXFLAGS += -msse4.1 -Wall -Wextra -Werror -std=c++1z -pthread

QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE += -Os
QMAKE_CXXFLAGS_DEBUG += -fsanitize=undefined-trap -fsanitize-undefined-trap-on-error
QMAKE_CXX = clang++

CONFIG(release, debug|release) {
  message( "Building for Release" )
}

CONFIG(debug, debug|release) {
  message( "Building for Debug" )
  DEFINES += DEBUG PROFILE
}


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
    ../lib/objects/wrappers/test/NonOwningPtr.cpp \
    ../lib/objects/wrappers/test/Any.cpp \
    ../lib/objects/wrappers/test/VectorizedArray.cpp \
    ../lib/objects/containers/test/Array.cpp \
    ../lib/objects/containers/test/ArrayUtils.cpp \
    ../lib/objects/finders/test/Order.cpp \
    ../lib/objects/finders/test/BruteForce.cpp \
    ../lib/sph/timestepping/test/TimeStepping.cpp \
    ../lib/sph/initial/test/Distribution.cpp \
    ../lib/sph/kernel/test/Kernel.cpp \
    ../lib/system/test/Settings.cpp \
    ../lib/system/test/Timer.cpp \
    ../lib/quantities/test/Quantity.cpp \
    ../lib/quantities/test/Storage.cpp \
    ../lib/quantities/test/Iterate.cpp \
    ../lib/quantities/test/QuantityHelpers.cpp \
    ../lib/objects/containers/test/BufferedArray.cpp \
    ../lib/objects/containers/test/Tuple.cpp \
    ../lib/objects/wrappers/test/Flags.cpp \
    ../lib/physics/test/Integrals.cpp \
    ../lib/geometry/test/Tensor.cpp \
    ../lib/system/test/Profiler.cpp \
    ../lib/objects/containers/test/LimitedArray.cpp \
    ../lib/objects/containers/test/StaticArray.cpp \
    ../lib/geometry/test/TracelessTensor.cpp \
    ../lib/system/test/Output.cpp \
    ../lib/solvers/test/ContinuitySolver.cpp \
    ../lib/sph/boundary/test/Boundary.cpp \
    ../lib/objects/wrappers/test/AlignedStorage.cpp \
    ../lib/solvers/test/Module.cpp \
    ../lib/geometry/test/Multipole.cpp \
    ../lib/sph/forces/test/Damage.cpp \
    ../lib/system/test/Statistics.cpp \
    ../lib/objects/finders/test/Finders.cpp \
    ../lib/sph/initial/test/Initial.cpp \
    ../lib/system/test/CallbackList.cpp \
    ../lib/sph/av/test/Balsara.cpp


HEADERS += \
    utils/Utils.h \
    utils/RecordType.h
