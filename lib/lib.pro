TEMPLATE = lib
CONFIG += c++14 staticLib thread silent
CONFIG -= app_bundle qt
CONFIG += staticlib
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
  DEFINES += SPH_PROFILE
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH LIB --- Building for Assert" )
  DEFINES += SPH_DEBUG SPH_PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH LIB --- Building for Debug" )
  DEFINES += SPH_DEBUG SPH_PROFILE
}


SOURCES += \
    math/rng/Rng.cpp \
    math/Morton.cpp \
    quantities/Storage.cpp \
    sph/boundary/Boundary.cpp \
    sph/initial/Distribution.cpp \
    sph/initial/Initial.cpp \
    timestepping/TimeStepping.cpp \
    objects/finders/Voxel.cpp \
    io/Logger.cpp \
    io/Output.cpp \
    system/Timer.cpp \
    system/Factory.cpp \
    system/Profiler.cpp \
    system/Settings.cpp \
    system/Statistics.cpp \
    physics/Eos.cpp \
    physics/TimeFormat.cpp \
    physics/Damage.cpp \
    physics/Rheology.cpp \
    post/Components.cpp \
    geometry/Domain.cpp \
    physics/Integrals.cpp \
    timestepping/TimeStepCriterion.cpp \
    objects/finders/KdTree.cpp \
    solvers/AbstractSolver.cpp \
    run/Run.cpp \
    sph/Diagnostics.cpp \
    common/Assert.cpp \
    io/filesystem.cpp \
    sph/Material.cpp \
    system/Platform.cpp \
    thread/CheckFunction.cpp \
    quantities/QuantityIds.cpp \
    objects/wrappers/Range.cpp \
    objects/containers/stringutils.cpp

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
    sph/kernel/Kernel.h \
    sph/initial/Distribution.h \
    timestepping/TimeStepping.h \
    sph/av/Standard.h \
    sph/boundary/Boundary.h \
    system/Callbacks.h \
    system/Factory.h \
    system/Settings.h \
    system/Timer.h \
    system/Profiler.h \
    io/Logger.h \
    io/Output.h \
    io/LogFile.h \
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
    objects/containers/StaticArray.h \
    sph/forces/CentripetalForce.h \
    objects/wrappers/AlignedStorage.h \
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
    sph/Diagnostics.h \
    system/filesystem.h \
    solvers/PhysicalStorage.h \
    common/ForwardDecl.h \
    common/Globals.h \
    common/Traits.h \
    system/FileSystem.h \
    quantities/AbstractMaterial.h \
    sph/Material.h \
    common/Assert.h \
    common/Assert.h \
    physics/Damage.h \
    physics/Rheology.h \
    solvers/Derivative.h \
    solvers/EquationTerm.h \
    objects/wrappers/Finally.h \
    system/Platform.h \
    io/FileSystem.h \
    objects/containers/ArrayMap.h \
    solvers/Accumulated.h \
    run/Run.h \
    solvers/GenericSolver.h \
    objects/containers/PerElementWrapper.h \
    thread/AtomicFloat.h \
    thread/CheckFunction.h \
    objects/Exceptions.h \
    system/Column.h \
    objects/containers/StringUtils.h \
    io/Column.h \
    solvers/XSph.h \
    solvers/GradH \
    sph/kernel/GravityKernel.h \
    sph/kernel/KernelFactory.h \
    solvers/GravitySolver.h
