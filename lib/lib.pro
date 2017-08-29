TEMPLATE = lib
CONFIG += c++14 staticlib thread silent
CONFIG -= app_bundle qt

# disable if you dont have eigen
INCLUDEPATH += /usr/include/eigen3
DEFINES += SPH_USE_EIGEN

#QMAKE_CXX = mpic++
#QMAKE_LINK = mpic++

QMAKE_CXXFLAGS += -Wall -Wextra -Werror -msse4.1 -std=c++14 -pthread
#QMAKE_CXXFLAGS_RELEASE -= -O2
#QMAKE_CXXFLAGS_RELEASE += -Os
#QMAKE_CXXFLAGS_DEBUG += -fsanitize=undefined-trap -fsanitize-undefined-trap-on-error  # -ftime-report



CONFIG(release, debug|profile|assert|release) {
  message( "SPH LIB --- Building for Release" )
  QMAKE_CXXFLAGS += -O4
}

CONFIG(profile, debug|profile|assert|release) {
  message( "SPH LIB --- Building for Profile" )
  DEFINES += SPH_PROFILE
  QMAKE_CXXFLAGS += -O4
}

CONFIG(assert, debug|profile|assert|release) {
  message( "SPH LIB --- Building for Assert" )
  DEFINES += SPH_DEBUG SPH_PROFILE
  QMAKE_CXXFLAGS += -O4
}

CONFIG(debug, debug|profile|assert|release) {
  message( "SPH LIB --- Building for Debug" )
  DEFINES += SPH_PROFILE SPH_DEBUG
}

SOURCES += \
    common/Assert.cpp \
    gravity/BarnesHut.cpp \
    gravity/VoxelGravity.cpp \
    io/FileSystem.cpp \
    io/Logger.cpp \
    io/Output.cpp \
    io/Path.cpp \
    math/Morton.cpp \
    math/SparseMatrix.cpp \
    math/rng/Rng.cpp \
    mpi/Mpi.cpp \
    objects/finders/KdTree.cpp \
    objects/finders/Voxel.cpp \
    objects/geometry/Domain.cpp \
    objects/geometry/SymmetricTensor.cpp \
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
    quantities/IMaterial.cpp \
    quantities/Particle.cpp \
    quantities/QuantityIds.cpp \
    quantities/Storage.cpp \
    run/IRun.cpp \
    sph/Diagnostics.cpp \
    sph/Material.cpp \
    sph/boundary/Boundary.cpp \
    sph/initial/Distribution.cpp \
    sph/initial/Initial.cpp \
    sph/solvers/StaticSolver.cpp \
    system/Factory.cpp \
    system/Platform.cpp \
    system/Process.cpp \
    system/Profiler.cpp \
    system/Settings.cpp \
    system/Statistics.cpp \
    system/Timer.cpp \
    tests/Setup.cpp \
    thread/CheckFunction.cpp \
    thread/Pool.cpp \
    timestepping/TimeStepCriterion.cpp \
    timestepping/TimeStepping.cpp \
    post/Plot.cpp

HEADERS += \
    common/Assert.h \
    common/ForwardDecl.h \
    common/Globals.h \
    common/Traits.h \
    gravity/BarnesHut.h \
    gravity/BruteForceGravity.h \
    gravity/IGravity.h \
    gravity/Moments.h \
    gravity/VoxelGravity.h \
    io/Column.h \
    io/FileSystem.h \
    io/LogFile.h \
    io/Logger.h \
    io/Output.h \
    io/Path.h \
    io/Serializer.h \
    math/Integrator.h \
    math/Math.h \
    math/Matrix.h \
    math/Means.h \
    math/Morton.h \
    math/Roots.h \
    math/SparseMatrix.h \
    math/rng/Rng.h \
    math/rng/VectorRng.h \
    mpi/Mpi.h \
    mpi/ProcessPool.h \
    mpi/Serializable.h \
    objects/Exceptions.h \
    objects/Object.h \
    objects/containers/Array.h \
    objects/containers/ArrayView.h \
    objects/containers/BufferedArray.h \
    objects/containers/List.h \
    objects/containers/LookupMap.h \
    objects/containers/Map.h \
    objects/containers/StaticArray.h \
    objects/containers/Tuple.h \
    objects/finders/BruteForceFinder.h \
    objects/finders/INeighbourFinder.h \
    objects/finders/KdTree.h \
    objects/finders/LinkedList.h \
    objects/finders/Linkedlist.h \
    objects/finders/Octree.h \
    objects/finders/Order.h \
    objects/finders/PeriodicFinder.h \
    objects/finders/Voxel.h \
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
    objects/utility/Iterators.h \
    objects/utility/OperatorTemplate.h \
    objects/utility/PerElementWrapper.h \
    objects/utility/StringUtils.h \
    objects/utility/Value.h \
    objects/wrappers/AlignedStorage.h \
    objects/wrappers/Any.h \
    objects/wrappers/AutoPtr.h \
    objects/wrappers/ClonePtr.h \
    objects/wrappers/Expected.h \
    objects/wrappers/Finally.h \
    objects/wrappers/Flags.h \
    objects/wrappers/Function.h \
    objects/wrappers/Interval.h \
    objects/wrappers/LockingPtr.h \
    objects/wrappers/NonOwningPtr.h \
    objects/wrappers/ObserverPtr.h \
    objects/wrappers/Optional.h \
    objects/wrappers/Outcome.h \
    objects/wrappers/RawPtr.h \
    objects/wrappers/SharedPtr.h \
    objects/wrappers/Variant.h \
    objects/wrappers/VectorizedArray.h \
    physics/Analytic.h \
    physics/Constants.h \
    physics/Damage.h \
    physics/Eos.h \
    physics/Integrals.h \
    physics/Rheology.h \
    physics/TimeFormat.h \
    post/Analysis.h \
    post/MarchingCubes.h \
    post/MeshFile.h \
    post/Plot.h \
    quantities/IMaterial.h \
    quantities/Iterate.h \
    quantities/Particle.h \
    quantities/Quantity.h \
    quantities/QuantityHelpers.h \
    quantities/QuantityIds.h \
    quantities/Storage.h \
    run/IRun.h \
    sph/Diagnostics.h \
    sph/Materials.h \
    sph/boundary/Boundary.h \
    sph/equations/Accumulated.h \
    sph/equations/Derivative.h \
    sph/equations/EquationTerm.h \
    sph/equations/Friction.h \
    sph/equations/GradH.h \
    sph/equations/HelperTerms.h \
    sph/equations/Potentials.h \
    sph/equations/Statics.h \
    sph/equations/XSph.h \
    sph/equations/av/Balsara.h \
    sph/equations/av/MorrisMonaghan.h \
    sph/equations/av/Riemann.h \
    sph/equations/av/Standard.h \
    sph/equations/av/Stress.h \
    sph/equations/heat/HeatDiffusion.h \
    sph/equationsav/Standard.h \
    sph/initial/Distribution.h \
    sph/initial/Initial.h \
    sph/kernel/GravityKernel.h \
    sph/kernel/Interpolation.h \
    sph/kernel/Kernel.h \
    sph/kernel/KernelFactory.h \
    sph/solvers/AsymmetricSolver.h \
    sph/solvers/ContinuitySolver.h \
    sph/solvers/DensityIndependentSolver.h \
    sph/solvers/EntropySolver.h \
    sph/solvers/GenericSolver.h \
    sph/solvers/GravitySolver.h \
    sph/solvers/StaticSolver.h \
    sph/solvers/SummationSolver.h \
    system/ArrayStats.h \
    system/Callbacks.h \
    system/Column.h \
    system/Element.h \
    system/Factory.h \
    system/Platform.h \
    system/Process.h \
    system/Profiler.h \
    system/Settings.h \
    system/Statistics.h \
    system/Timer.h \
    tests/Approx.h \
    tests/Setup.h \
    thread/AtomicFloat.h \
    thread/CheckFunction.h \
    thread/Pool.h \
    thread/ThreadLocal.h \
    timestepping/ISolverr.h \
    timestepping/TimeStepCriterion.h \
    timestepping/TimeStepping.h \
    objects/finders/DynamicFinder.h
