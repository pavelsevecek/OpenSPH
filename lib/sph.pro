TEMPLATE = lib
CONFIG += c++14 staticLib thread
CONFIG -= app_bundle qt
QMAKE_CXXFLAGS += -Wall -Wextra -Werror -msse4.1 -std=c++14 -pthread
QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE += -Os
QMAKE_CXX = g++

SOURCES += \
    physics/TimeFormat.cpp \
    sph/initial/Distribution.cpp \
    system/Timer.cpp \
    system/Factory.cpp \
    sph/timestepping/TimeStepping.cpp \
    system/Profiler.cpp \
    solvers/SolverFactory.cpp \
    system/Settings.cpp \
    math/rng/Rng.cpp \
    quantities/Material.cpp \
    quantities/Storage.cpp \
    sph/boundary/Boundary.cpp \
    sph/initial/Initial.cpp \
    sph/forces/Damage.cpp \
    objects/finders/Voxel.cpp \
    system/Logger.cpp \
    system/Output.cpp \
    physics/Eos.cpp

HEADERS += \
    core/Globals.h \
    core/Traits.h \
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
    problem/Problem.h \
    sph/kernel/Kernel.h \
    sph/initial/Distribution.h \
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
    system/CallbackList.h \
    system/Statistics.h \
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
    quantities/QuantityKey.h \
    solvers/Accumulator.h \
    solvers/SharedAccumulator.h \
    objects/containers/StaticArray.h \
    sph/av/Factory.h \
    quantities/Material.h \
    sph/forces/CentripetalForce.h \
    sph/forces/Factory.h \
    sph/forces/Damage.h \
    sph/forces/Yielding.h \
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
    objects/containers/LookupMap.h
