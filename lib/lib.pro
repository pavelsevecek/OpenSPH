TEMPLATE = lib
CONFIG += c++14 staticlib thread silent
CONFIG -= app_bundle qt

# disable if you dont have eigen
INCLUDEPATH += /usr/include/eigen3
DEFINES += SPH_USE_EIGEN
#DEFINES += SPH_MPI

#QMAKE_CXX = clang++
#QMAKE_LINK = clang++

QMAKE_CXXFLAGS += -Wall -Wextra -Werror -msse4.1 -std=c++14 -pthread
QMAKE_LFLAGS += -static-libgcc -static-libstdc++
#QMAKE_CXXFLAGS_RELEASE -= -O2
#QMAKE_CXXFLAGS_RELEASE += -Os
#QMAKE_CXXFLAGS_DEBUG += -fsanitize=undefined-trap -fsanitize-undefined-trap-on-error  # -ftime-report


CONFIG(release, debug|profile|assert|release) {
  message( "SPH LIB --- Building for Release" )
  QMAKE_CXXFLAGS += -O2
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH LIB --- Building for Profile" )
  DEFINES += SPH_PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH LIB --- Building for Assert" )
  DEFINES += SPH_DEBUG SPH_PROFILE
  QMAKE_CXXFLAGS += -O2
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH LIB --- Building for Debug" )
  DEFINES += SPH_PROFILE SPH_DEBUG
}

SOURCES += \
    common/Assert.cpp \
    gravity/BarnesHut.cpp \
    gravity/NBodySolver.cpp \
    io/FileManager.cpp \
    io/FileSystem.cpp \
    io/Logger.cpp \
    io/Output.cpp \
    io/Path.cpp \
    math/Morton.cpp \
    math/SparseMatrix.cpp \
    math/rng/Rng.cpp \
    mpi/Mpi.cpp \
    objects/containers/String.cpp \
    objects/finders/Bvh.cpp \
    objects/finders/DynamicFinder.cpp \
    objects/finders/KdTree.cpp \
    objects/finders/UniformGrid.cpp \
    objects/geometry/Domain.cpp \
    objects/geometry/SymmetricTensor.cpp \
    objects/utility/Dynamic.cpp \
    objects/utility/StringUtils.cpp \
    objects/wrappers/Interval.cpp \
    physics/Damage.cpp \
    physics/Eos.cpp \
    physics/Integrals.cpp \
    physics/Rheology.cpp \
    physics/TimeFormat.cpp \
    post/Analysis.cpp \
    post/MarchingCubes.cpp \
    post/MeshFile.cpp \
    post/Plot.cpp \
    post/StatisticTests.cpp \
    quantities/IMaterial.cpp \
    quantities/Particle.cpp \
    quantities/QuantityIds.cpp \
    quantities/Storage.cpp \
    run/IRun.cpp \
    sph/Diagnostics.cpp \
    sph/Material.cpp \
    sph/boundary/Boundary.cpp \
    sph/equations/EquationTerm.cpp \
    sph/equations/Rotation.cpp \
    sph/initial/Distribution.cpp \
    sph/initial/Initial.cpp \
    sph/solvers/AsymmetricSolver.cpp \
    sph/solvers/GravitySolver.cpp \
    sph/solvers/StandardSets.cpp \
    sph/solvers/StaticSolver.cpp \
    sph/solvers/SymmetricSolver.cpp \
    system/ArgsParser.cpp \
    system/Factory.cpp \
    system/Platform.cpp \
    system/Process.cpp \
    system/Profiler.cpp \
    system/Statistics.cpp \
    system/Timer.cpp \
    tests/Setup.cpp \
    thread/CheckFunction.cpp \
    thread/Pool.cpp \
    timestepping/TimeStepCriterion.cpp \
    timestepping/TimeStepping.cpp \
    sph/equations/Derivative.cpp \
    sph/solvers/SummationSolver.cpp \
    sph/initial/Presets.cpp \
    system/Settings.cpp \
    run/Collision.cpp \
    sph/solvers/GradHSolver.cpp

HEADERS += \
    common/Assert.h \
    common/ForwardDecl.h \
    common/Globals.h \
    common/Traits.h \
    gravity/BarnesHut.h \
    gravity/BruteForceGravity.h \
    gravity/Collision.h \
    gravity/IGravity.h \
    gravity/Moments.h \
    gravity/NBodySolver.h \
    gravity/SphericalGravity.h \
    io/Column.h \
    io/FileManager.h \
    io/FileSystem.h \
    io/LogFile.h \
    io/Logger.h \
    io/Output.h \
    io/Path.h \
    io/Serializer.h \
    io/Table.h \
    math/AffineMatrix.h \
    math/Math.h \
    math/Matrix.h \
    math/Means.h \
    math/Morton.h \
    math/Quat.h \
    math/SparseMatrix.h \
    math/rng/Rng.h \
    math/rng/VectorRng.h \
    mpi/Mpi.h \
    mpi/MpiScheduler.h \
    mpi/Serializable.h \
    objects/Exceptions.h \
    objects/Object.h \
    objects/containers/Allocators.h \
    objects/containers/Array.h \
    objects/containers/ArrayRef.h \
    objects/containers/ArrayView.h \
    objects/containers/BufferedArray.h \
    objects/containers/FlatMap.h \
    objects/containers/FlatSet.h \
    objects/containers/List.h \
    objects/containers/LookupMap.h \
    objects/containers/StaticArray.h \
    objects/containers/String.h \
    objects/containers/Tuple.h \
    objects/finders/AdaptiveGrid.h \
    objects/finders/BruteForceFinder.h \
    objects/finders/Bvh.h \
    objects/finders/DynamicFinder.h \
    objects/finders/KdTree.h \
    objects/finders/LinkedList.h \
    objects/finders/Linkedlist.h \
    objects/finders/NeighbourFinder.h \
    objects/finders/Octree.h \
    objects/finders/Order.h \
    objects/finders/PeriodicFinder.h \
    objects/finders/UniformGrid.h \
    objects/geometry/AntisymmetricTensor.h \
    objects/geometry/Box.h \
    objects/geometry/Domagin.h \
    objects/geometry/Domain.h \
    objects/geometry/Generic.h \
    objects/geometry/Indices.h \
    objects/geometry/Multipole.h \
    objects/geometry/Sphere.h \
    objects/geometry/SymmetricTensor.h \
    objects/geometry/Tensor.h \
    objects/geometry/TracelessTensor.h \
    objects/geometry/Vector.h \
    objects/utility/ArrayUtils.h \
    objects/utility/Dynamic.h \
    objects/utility/Iterators.h \
    objects/utility/OperatorTemplate.h \
    objects/utility/PerElementWrapper.h \
    objects/utility/StringUtils.h \
    objects/wrappers/AlignedStorage.h \
    objects/wrappers/Any.h \
    objects/wrappers/AutoPtr.h \
    objects/wrappers/ClonePtr.h \
    objects/wrappers/Expected.h \
    objects/wrappers/Finally.h \
    objects/wrappers/Flags.h \
    objects/wrappers/Function.h \
    objects/wrappers/Interval.h \
    objects/wrappers/Locking.h \
    objects/wrappers/LockingPtr.h \
    objects/wrappers/Lut.h \
    objects/wrappers/NonOwningPtr.h \
    objects/wrappers/ObserverPtr.h \
    objects/wrappers/Optional.h \
    objects/wrappers/Outcome.h \
    objects/wrappers/RawPtr.h \
    objects/wrappers/SafePtr.h \
    objects/wrappers/SharedPtr.h \
    objects/wrappers/Transient.h \
    objects/wrappers/Variant.h \
    objects/wrappers/VectorizedArray.h \
    physics/Constants.h \
    physics/Damage.h \
    physics/Eos.h \
    physics/Functions.h \
    physics/Integrals.h \
    physics/Rheology.h \
    physics/TimeFormat.h \
    physics/Units.h \
    post/Analysis.h \
    post/MarchingCubes.h \
    post/MeshFile.h \
    post/Plot.h \
    post/Point.h \
    post/StatisticTests.h \
    quantities/IMaterial.h \
    quantities/Iterate.h \
    quantities/Particle.h \
    quantities/Quantity.h \
    quantities/QuantityHelpers.h \
    quantities/QuantityIds.h \
    quantities/Storage.h \
    run/Collision.h \
    run/CompositeRun.h \
    run/IRun.h \
    run/RunCallbacks.h \
    run/Trigger.h \
    sph/Diagnostics.h \
    sph/Materials.h \
    sph/boundary/Boundary.h \
    sph/equations/Accumulated.h \
    sph/equations/Derivative.h \
    sph/equations/EquationTerm.h \
    sph/equations/Fluids.h \
    sph/equations/Friction.h \
    sph/equations/Heat.h \
    sph/equations/HelperTerms.h \
    sph/equations/Potentials.h \
    sph/equations/Rotation.h \
    sph/equations/XSph.h \
    sph/equations/av/Balsara.h \
    sph/equations/av/MorrisMonaghan.h \
    sph/equations/av/Riemann.h \
    sph/equations/av/Standard.h \
    sph/equations/av/Stress.h \
    sph/equations/heat/Heat.h \
    sph/equationsav/Standard.h \
    sph/initial/Distribution.h \
    sph/initial/Initial.h \
    sph/initial/Presets.h \
    sph/kernel/GravityKernel.h \
    sph/kernel/Interpolation.h \
    sph/kernel/Kernel.h \
    sph/solvers/AsymmetricSolver.h \
    sph/solvers/CollisionSolver.h \
    sph/solvers/DensityIndependentSolver.h \
    sph/solvers/EntropySolver.h \
    sph/solvers/GravitySolver.h \
    sph/solvers/StabilizationSolver.h \
    sph/solvers/StandardSets.h \
    sph/solvers/StaticSolver.h \
    sph/solvers/SummationSolver.h \
    sph/solvers/SymmetricSolver.h \
    system/ArgsParser.h \
    system/ArrayStats.h \
    system/Column.h \
    system/Element.h \
    system/Factory.h \
    system/Platform.h \
    system/Process.h \
    system/Profiler.h \
    system/RunCallbacks.h \
    system/Settings.h \
    system/Statistics.h \
    system/Timer.h \
    tests/Approx.h \
    tests/Setup.h \
    thread/AtomicFloat.h \
    thread/CheckFunction.h \
    thread/IScheduler.h \
    thread/Pool.h \
    thread/ThreadLocal.h \
    timestepping/ISolver.h \
    timestepping/ISolverr.h \
    timestepping/TimeStepCriterion.h \
    timestepping/TimeStepping.h \
    io/OutOfCore.h \
    objects/utility/EnumMap.h \
    system/Settings.impl.h \
    math/Functional.h \
    sph/solvers/GradHSolver.h
