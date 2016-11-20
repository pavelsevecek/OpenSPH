TEMPLATE = lib
CONFIG += c++14 staticLib thread
CONFIG -= app_bundle qt
QMAKE_CXXFLAGS += -Wall -msse4.1 -std=c++14 -pthread -pg
QMAKE_LFLAGS += -pg

SOURCES += \
    physics/TimeFormat.cpp \
    sph/distributions/Distribution.cpp \
    system/Timer.cpp \
    system/Factory.cpp \
    sph/timestepping/TimeStepping.cpp \
    models/BasicModel.cpp \
    system/Profiler.cpp

HEADERS += \
    core/Globals.h \
    core/Traits.h \
    geometry/Bounds.h \
    geometry/Domain.h \
    geometry/Indices.h \
    geometry/Vector.h \
    geometry/Tensor.h \
    math/Math.h \
    math/Integrator.h \
    math/rng/Rng.h \
    math/rng/VectorRng.h \
    objects/Object.h \
    objects/containers/Array.h \
    objects/containers/ArrayUtils.h \
    objects/containers/ArrayView.h \
    objects/containers/Tuple.h \
    objects/containers/LimitedArray.h \
    objects/containers/BufferedArray.h \
    objects/finders/Bruteforce.h \
    objects/finders/Finder.h \
    objects/finders/KdTree.h \
    objects/finders/Linkedlist.h \
    objects/finders/Nanoflann.h \
    objects/finders/Octree.h \
    objects/finders/Order.h \
    objects/finders/PeriodicFinder.h \
    objects/wrappers/Any.h \
    objects/wrappers/Flags.h \
    objects/wrappers/Iterators.h \
    objects/wrappers/NonOwningPtr.h \
    objects/wrappers/Optional.h \
    objects/wrappers/Range.h \
    objects/wrappers/Shadow.h \
    objects/wrappers/Variant.h \
    objects/wrappers/VectorizedArray.h \
    physics/Constants.h \
    physics/Eos.h \
    physics/TimeFormat.h \
    physics/Integrals.h \
    problem/Problem.h \
    sph/kernel/Kernel.h \
    sph/distributions/Distribution.h \
    sph/timestepping/Step.h \
    sph/timestepping/TimeStepping.h \
    sph/av/Monaghan.h \
    sph/boundary/Boundary.h \
    system/Callbacks.h \
    system/Factory.h \
    system/Logger.h \
    system/Parser.h \
    system/Settings.h \
    system/Timer.h \
    system/Profiler.h \
    models/AbstractModel.h \
    models/BasicModel.h \
    models/CompositeModel.h \
    storage/Iterate.h \
    storage/Storage.h \
    storage/Quantity.h \
    storage/QuantityHelpers.h \
    storage/QuantityMap.h \
    geometry/TracelessTensor.h \
    system/Output.h
