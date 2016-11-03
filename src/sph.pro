TEMPLATE = lib
CONFIG += c++14 staticLib thread
CONFIG -= app_bundle qt
QMAKE_CXXFLAGS += -Wall -msse4.1 -std=c++14 -pthread
SOURCES += \
    physics/TimeFormat.cpp \
    sph/initconds/InitConds.cpp \
    system/Timer.cpp \
    system/Factory.cpp \
    physics/TimeStepping.cpp \
    models/BasicModel.cpp \
    storage/BasicView.cpp \
    objects/wrappers/test/any.cpp

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
    objects/finders/Bruteforce.h \
    objects/finders/Finder.h \
    objects/finders/KdTree.h \
    objects/finders/Linkedlist.h \
    objects/finders/Nanoflann.h \
    objects/finders/Octree.h \
    objects/finders/Order.h \
    objects/wrappers/Flags.h \
    objects/wrappers/Iterators.h \
    objects/wrappers/NonOwningPtr.h \
    objects/wrappers/Optional.h \
    objects/wrappers/Range.h \
    objects/wrappers/Shadow.h \
    objects/wrappers/Variant.h \
    physics/Constants.h \
    physics/Eos.h \
    physics/TimeFormat.h \
    physics/TimeStepping.h \
    problem/Problem.h \
    sph/initconds/InitConds.h \
    sph/kernel/Kernel.h \
    objects/containers/Array.h \
    system/Callbacks.h \
    system/Factory.h \
    system/Logger.h \
    system/Parser.h \
    system/Settings.h \
    system/Timer.h \
    models/BasicModel.h \
    storage/GenericStorage.h \
    storage/BasicView.h \
    storage/Iterables.h \
    objects/wrappers/Any.h
