TEMPLATE = lib
CONFIG += c++14 staticLib thread
CONFIG -= app_bundle qt
QMAKE_CXXFLAGS += -Wall -msse4.1 -mavx -std=c++14 -pthread -pg
QMAKE_LFLAGS += -pg

#CONFIG(release, debug|profile|release) {
#  message( "Building for Release" )
#}
#CONFIG(profile, debug|profile|release) {
#  message( "Building for Profile" )
#  DEFINES += PROFILE
#}
#CONFIG(debug, debug|profile|release) {
#  message( "Building for Debug" )
#  DEFINES += DEBUG PROFILE
#}


SOURCES += \
    physics/TimeFormat.cpp \
    sph/distributions/Distribution.cpp \
    system/Timer.cpp \
    system/Factory.cpp \
    sph/timestepping/TimeStepping.cpp \
    system/Profiler.cpp \
    solvers/ContinuitySolver.cpp \
    solvers/SummationSolver.cpp \
    solvers/AbstractSolver.cpp \
    solvers/SolverFactory.cpp \
    system/Settings.cpp

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
    sph/av/Standard.h \
    sph/boundary/Boundary.h \
    system/Callbacks.h \
    system/Factory.h \
    system/Logger.h \
    system/Settings.h \
    system/Timer.h \
    system/Profiler.h \
    storage/Iterate.h \
    storage/Storage.h \
    storage/Quantity.h \
    storage/QuantityHelpers.h \
    geometry/TracelessTensor.h \
    system/Output.h \
    solvers/AbstractSolver.h \
    solvers/ContinuitySolver.h \
    solvers/SummationSolver.h \
    sph/av/Riemann.h \
    sph/av/MorrisMonaghan.h \
    sph/av/Balsara.h \
    sph/forces/StressForce.h \
    solvers/DensityIndependentSolver.h \
    solvers/EntropySolver.h \
    storage/QuantityKey.h \
    solvers/Accumulator.h \
    solvers/SharedAccumulator.h \
    objects/containers/StaticArray.h \
    sph/av/Factory.h \
    storage/QuantityMap.h \
    storage/Material.h \
    sph/forces/CentripetalForce.h \
    sph/forces/Factory.h \
    sph/forces/Damage.h \
    sph/forces/Yielding.h \
    solvers/SolverFactory.h
