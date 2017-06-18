TEMPLATE = lib
CONFIG += c++14 staticLib thread silent
CONFIG -= app_bundle qt
CONFIG += staticlib

# disable if you don't have eigen
INCLUDEPATH += /usr/include/eigen3
DEFINES += SPH_USE_EIGEN

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
    common/Assert.cpp \
    geometry/Domain.cpp \
    io/Logger.cpp \
    io/Output.cpp \
    math/Morton.cpp \
    math/rng/Rng.cpp \
    objects/finders/KdTree.cpp \
    objects/finders/Voxel.cpp \
    objects/wrappers/Range.cpp \
    physics/Damage.cpp \
    physics/Eos.cpp \
    physics/Integrals.cpp \
    physics/Rheology.cpp \
    physics/TimeFormat.cpp \
    post/Components.cpp \
    quantities/QuantityIds.cpp \
    quantities/Storage.cpp \
    run/Run.cpp \
    sph/Diagnostics.cpp \
    sph/Material.cpp \
    sph/boundary/Boundary.cpp \
    sph/initial/Distribution.cpp \
    sph/initial/Initial.cpp \
    system/Factory.cpp \
    system/Platform.cpp \
    system/Profiler.cpp \
    system/Settings.cpp \
    system/Statistics.cpp \
    system/Timer.cpp \
    thread/CheckFunction.cpp \
    timestepping/TimeStepCriterion.cpp \
    timestepping/TimeStepping.cpp \
    io/FileSystem.cpp \
    math/SparseMatrix.cpp \
    geometry/SymmetricTensor.cpp \
    sph/solvers/StaticSolver.cpp \
    objects/containers/StringUtils.cpp \
    io/Path.cpp \
    tests/Setup.cpp

HEADERS += \
    commmon/ForwardDecl.h \
    commmon/Globals.h \
    commmon/Traits.h \
    common/Assert.h \
    common/ForwardDecl.h \
    common/Globals.h \
    common/Traits.h \
    geometry/Box.h \
    geometry/Domain.h \
    geometry/Indices.h \
    geometry/Multipole.h \
    geometry/TracelessTensor.h \
    geometry/Vector.h \
    io/Column.h \
    io/FileSystem.h \
    io/LogFile.h \
    io/Logger.h \
    io/Output.h \
    math/Integrator.h \
    math/Math.h \
    math/Means.h \
    math/Morton.h \
    math/rng/Rng.h \
    math/rng/VectorRng.h \
    objects/Exceptions.h \
    objects/Object.h \
    objects/containers/Array.h \
    objects/containers/ArrayMap.h \
    objects/containers/ArrayUtils.h \
    objects/containers/ArrayView.h \
    objects/containers/BufferedArray.h \
    objects/containers/LookupMap.h \
    objects/containers/PerElementWrapper.h \
    objects/containers/StaticArray.h \
    objects/containers/StringUtils.h \
    objects/containers/Tuple.h \
    objects/finders/AbstractFinder.h \
    objects/finders/Bruteforce.h \
    objects/finders/KdTree.h \
    objects/finders/LinkedList.h \
    objects/finders/Linkedlist.h \
    objects/finders/Nanoflann.h \
    objects/finders/Octree.h \
    objects/finders/Order.h \
    objects/finders/PeriodicFinder.h \
    objects/finders/Voxel.h \
    objects/wrappers/AlignedStorage.h \
    objects/wrappers/Any.h \
    objects/wrappers/Expected.h \
    objects/wrappers/Finally.h \
    objects/wrappers/Flags.h \
    objects/wrappers/Iterators.h \
    objects/wrappers/NonOwningPtr.h \
    objects/wrappers/ObserverPtr.h \
    objects/wrappers/Optional.h \
    objects/wrappers/Outcome.h \
    objects/wrappers/Range.h \
    objects/wrappers/Value.h \
    objects/wrappers/Variant.h \
    objects/wrappers/VectorizedArray.h \
    physics/Constants.h \
    physics/Damage.h \
    physics/Eos.h \
    physics/Integrals.h \
    physics/Rheology.h \
    physics/TimeFormat.h \
    post/Components.h \
    quantities/AbstractMaterial.h \
    quantities/Iterate.h \
    quantities/Quantity.h \
    quantities/QuantityHelpers.h \
    quantities/QuantityIds.h \
    quantities/Storage.h \
    run/Run.h \
    sph/Diagnostics.h \
    sph/Material.h \
    sph/boundary/Boundary.h \
    sph/equations/Accumulated.h \
    sph/equations/Derivative.h \
    sph/equations/EquationTerm.h \
    sph/equations/GradH.h \
    sph/equations/XSph.h \
    sph/equations/av/Balsara.h \
    sph/equations/av/MorrisMonaghan.h \
    sph/equations/av/Riemann.h \
    sph/equationsav/Standard.h \
    sph/initial/Distribution.h \
    sph/initial/Initial.h \
    sph/kernel/GravityKernel.h \
    sph/kernel/Kernel.h \
    sph/kernel/KernelFactory.h \
    sph/solvers/ContinuitySolver.h \
    sph/solvers/DensityIndependentSolver.h \
    sph/solvers/EntropySolver.h \
    sph/solvers/GenericSolver.h \
    sph/solvers/GravitySolver.h \
    sph/solvers/SummationSolver.h \
    system/ArrayStats.h \
    system/Callbacks.h \
    system/Column.h \
    system/Element.h \
    system/Factory.h \
    system/Platform.h \
    system/Profiler.h \
    system/Settings.h \
    system/Statistics.h \
    system/Timer.h \
    thread/AtomicFloat.h \
    thread/CheckFunction.h \
    thread/Pool.h \
    thread/ThreadLocal.h \
    timestepping/AbstractSolver.h \
    timestepping/TimeStepCriterion.h \
    timestepping/TimeStepping.h \
    sph/equations/heat/HeatDiffusion.h \
    sph/equations/av/Standard.h \
    objects/wrappers/AutoPtr.h \
    objects/wrappers/SharedPtr.h \
    objects/wrappers/LockingPtr.h \
    geometry/Generic.h \
    post/Plot.h \
    gravity/BarnesHut.h \
    math/SparseMatrix.h \
    sph/equations/Potentials.h \
    sph/equations/Statics.h \
    sph/solvers/StaticSolver.h \
    objects/finders/BruteForce.h \
    geometry/AntisymmetricTensor.h \
    sph/kernel/Interpolation.h \
    sph/equations/Friction.h \
    sph/equations/HelperTerms.h \
    math/Matrix.h \
    geometry/SymmetricTensor.h \
    geometry/Tensor.h \
    sph/equations/av/Stress.h \
    objects/wrappers/ClonePtr.h \
    math/Roots.h \
    physics/Analytic.h \
    io/Path.h \
    tests/Setup.h
