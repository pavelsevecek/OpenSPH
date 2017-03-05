TEMPLATE = lib
CONFIG += c++14 staticLib thread silent
CONFIG -= app_bundle qt
QMAKE_CXXFLAGS += -Wall -Wextra -Werror -msse4.1 -std=c++14 -pthread
#QMAKE_CXXFLAGS_RELEASE -= -O2
#QMAKE_CXXFLAGS_RELEASE += -Os
#QMAKE_CXXFLAGS_DEBUG += -fsanitize=undefined-trap -fsanitize-undefined-trap-on-error  # -ftime-report
QMAKE_CXX = g++


CONFIG(release, debug|profile|assert|release) {
  message( "SPH LIB --- Building for Release" )
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH LIB --- Building for Profile" )
  DEFINES += PROFILE
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH LIB --- Building for Assert" )
  DEFINES += DEBUG PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH LIB --- Building for Debug" )
  DEFINES += DEBUG PROFILE
}


SOURCES += \
    solvers/SolverFactory.cpp \
    math/rng/Rng.cpp \
    math/Morton.cpp \
    quantities/Material.cpp \
    quantities/Storage.cpp \
    sph/boundary/Boundary.cpp \
    sph/initial/Distribution.cpp \
    sph/initial/Initial.cpp \
    timestepping/TimeStepping.cpp \
    objects/finders/Voxel.cpp \
    system/Logger.cpp \
    system/Output.cpp \
    system/Timer.cpp \
    system/Factory.cpp \
    system/Profiler.cpp \
    system/Settings.cpp \
    physics/Eos.cpp \
    physics/TimeFormat.cpp \
    physics/Damage.cpp \
    physics/Yielding.cpp \
    post/Components.cpp \
    geometry/Domain.cpp \
    physics/Integrals.cpp \
    timestepping/TimeStepCriterion.cpp \
    objects/finders/KdTree.cpp \
    system/Statistics.cpp \
    solvers/AbstractSolver.cpp \
    problem/Problem.cpp \
    sph/Diagnostics.cpp \
    geometry/test/Box.cpp \
    geometry/test/Domain.cpp \
    geometry/test/Indices.cpp \
    geometry/test/Multipole.cpp \
    geometry/test/Tensor.cpp \
    geometry/test/TracelessTensor.cpp \
    geometry/test/Vector.cpp \
    math/rng/test/Rng.cpp \
    math/rng/test/VectorRng.cpp \
    math/test/Integrator.cpp \
    math/test/Math.cpp \
    math/test/Morton.cpp \
    objects/containers/test/Array.cpp \
    objects/containers/test/ArrayUtils.cpp \
    objects/containers/test/BufferedArray.cpp \
    objects/containers/test/StaticArray.cpp \
    objects/containers/test/Tuple.cpp \
    objects/finders/benchmark/Finders.cpp \
    objects/finders/test/BruteForce.cpp \
    objects/finders/test/Finders.cpp \
    objects/finders/test/Order.cpp \
    objects/wrappers/test/AlignedStorage.cpp \
    objects/wrappers/test/Any.cpp \
    objects/wrappers/test/Flags.cpp \
    objects/wrappers/test/Iterators.cpp \
    objects/wrappers/test/NonOwningPtr.cpp \
    objects/wrappers/test/Optional.cpp \
    objects/wrappers/test/Range.cpp \
    objects/wrappers/test/Variant.cpp \
    objects/wrappers/test/VectorizedArray.cpp \
    physics/test/Damage.cpp \
    physics/test/Eos.cpp \
    physics/test/Integrals.cpp \
    physics/test/TimeFormat.cpp \
    physics/test/Yielding.cpp \
    post/test/Components.cpp \
    problem/test/Problem.cpp \
    quantities/test/Iterate.cpp \
    quantities/test/Material.cpp \
    quantities/test/Quantity.cpp \
    quantities/test/QuantityHelpers.cpp \
    quantities/test/Storage.cpp \
    solvers/benchmark/ContinuitySolver.cpp \
    solvers/test/Accumulator.cpp \
    solvers/test/ContinuitySolver.cpp \
    solvers/test/Module.cpp \
    sph/av/test/Balsara.cpp \
    sph/boundary/test/Boundary.cpp \
    sph/initial/test/Distribution.cpp \
    sph/initial/test/Initial.cpp \
    sph/kernel/test/Kernel.cpp \
    sph/test/Diagnostics.cpp \
    system/test/ArrayStats.cpp \
    system/test/CallbackList.cpp \
    system/test/Output.cpp \
    system/test/Profiler.cpp \
    system/test/Settings.cpp \
    system/test/Statistics.cpp \
    system/test/Timer.cpp \
    thread/test/Pool.cpp \
    timestepping/test/TimeStepCriterion.cpp \
    timestepping/test/TimeStepping.cpp

HEADERS += \
    commmon/Globals.h \
    commmon/Traits.h \
    commmon/ForwardDecl.h \
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
    objects/containers/BufferedArray.h \
    objects/finders/Bruteforce.h \
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
    objects/wrappers/Variant.h \
    objects/wrappers/VectorizedArray.h \
    physics/Constants.h \
    physics/Eos.h \
    physics/TimeFormat.h \
    physics/Integrals.h \
    physics/Yielding.h \
    physics/Damage.h \
    problem/Problem.h \
    sph/kernel/Kernel.h \
    sph/initial/Distribution.h \
    timestepping/TimeStepping.h \
    sph/av/Standard.h \
    sph/boundary/Boundary.h \
    system/Callbacks.h \
    system/Factory.h \
    system/Logger.h \
    system/Settings.h \
    system/Timer.h \
    system/Profiler.h \
    system/CallbackList.h \
    system/Output.h \
    quantities/Iterate.h \
    quantities/Storage.h \
    quantities/Quantity.h \
    quantities/QuantityHelpers.h \
    geometry/TracelessTensor.h \
    solvers/AbstractSolver.h \
    solvers/ContinuitySolver.h \
    solvers/SummationSolver.h \
    sph/av/Riemann.h \
    sph/av/MorrisMonaghan.h \
    sph/av/Balsara.h \
    sph/forces/StressForce.h \
    solvers/DensityIndependentSolver.h \
    solvers/EntropySolver.h \
    solvers/Accumulator.h \
    solvers/SharedAccumulator.h \
    objects/containers/StaticArray.h \
    sph/av/Factory.h \
    quantities/Material.h \
    sph/forces/CentripetalForce.h \
    sph/forces/Factory.h \
    solvers/SolverFactory.h \
    objects/wrappers/AlignedStorage.h \
    solvers/Module.h \
    sph/initial/Initial.h \
    geometry/Multipole.h \
    objects/finders/LinkedList.h \
    objects/finders/BruteForce.h \
    objects/finders/Voxel.h \
    objects/finders/AbstractFinder.h \
    geometry/Box.h \
    objects/containers/LookupMap.h \
    post/Components.h \
    math/Morton.h \
    system/ArrayStats.h \
    system/Statistics.h \
    objects/wrappers/UniquePtr.h \
    quantities/QuantityIds.h \
    thread/Pool.h \
    thread/ConcurrentQueue.h \
    thread/ThreadLocal.h \
    objects/wrappers/Value.h \
    objects/wrappers/Expected.h \
    objects/wrappers/Outcome.h \
    timestepping/TimeStepCriterion.h \
    objects/wrappers/ObserverPtr.h \
    math/Means.h \
    system/Element.h \
    quantities/Slice.h \
    objects/ExtendEnum.h \
    system/LogFile.h \
    sph/Diagnostics.h \
    solvers/PressureForce.h \
    system/filesystem.h \
    solvers/SphSolver.h \
    solvers/PhysicalStorage.h \
    common/ForwardDecl.h \
    common/Globals.h \
    common/Traits.h \
    system/FileSystem.h
