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
    objects/geometry/Domain.cpp \
    objects/geometry/SymmetricTensor.cpp \
    io/FileSystem.cpp \
    io/Logger.cpp \
    io/Output.cpp \
    io/Path.cpp \
    math/Morton.cpp \
    math/SparseMatrix.cpp \
    math/rng/Rng.cpp \
    objects/utility/StringUtils.cpp \
    objects/finders/Voxel.cpp \
    physics/Damage.cpp \
    physics/Eos.cpp \
    physics/Integrals.cpp \
    physics/Rheology.cpp \
    physics/TimeFormat.cpp \
    quantities/QuantityIds.cpp \
    quantities/Storage.cpp \
    sph/Diagnostics.cpp \
    sph/Material.cpp \
    sph/boundary/Boundary.cpp \
    sph/initial/Distribution.cpp \
    sph/initial/Initial.cpp \
    sph/solvers/StaticSolver.cpp \
    system/Factory.cpp \
    system/Platform.cpp \
    system/Profiler.cpp \
    system/Settings.cpp \
    system/Statistics.cpp \
    system/Timer.cpp \
    tests/Setup.cpp \
    thread/CheckFunction.cpp \
    timestepping/TimeStepCriterion.cpp \
    timestepping/TimeStepping.cpp \
    quantities/Particle.cpp \
    post/MarchingCubes.cpp \
    post/MeshFile.cpp \
    objects/finders/KdTree.cpp \
    gravity/BarnesHut.cpp \
    thread/Pool.cpp \
    gravity/VoxelGravity.cpp \
    system/Process.cpp \
    mpi/Mpi.cpp \
    post/Analysis.cpp \
    objects/wrappers/Interval.cpp \
    run/IRun.cpp

HEADERS += \
    commmon/ForwardDecl.h \
    commmon/Globals.h \
    commmon/Traits.h \
    common/Assert.h \
    common/ForwardDecl.h \
    common/Globals.h \
    common/Traits.h \
    gravity/BarnesHut.h \
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
    objects/Exceptions.h \
    objects/Object.h \
    objects/geometry/AntisymmetricTensor.h \
    objects/geometry/Box.h \
    objects/geometry/Domagin.h \
    objects/geometry/Generic.h \
    objects/geometry/Indices.h \
    objects/geometry/Multipole.h \
    objects/geometry/SymmetricTensor.h \
    objects/geometry/Tensor.h \
    objects/geometry/TracelessTensor.h \
    objects/geometry/Vector.h \
    objects/containers/Array.h \
    objects/utility/ArrayUtils.h \
    objects/containers/ArrayView.h \
    objects/containers/BufferedArray.h \
    objects/containers/LookupMap.h \
    objects/utility/PerElementWrapper.h \
    objects/containers/StaticArray.h \
    objects/utility/StringUtils.h \
    objects/containers/Tuple.h \
    objects/finders/BruteForceFinder.h \
    objects/finders/LinkedList.h \
    objects/finders/Linkedlist.h \
    objects/finders/Octree.h \
    objects/finders/Order.h \
    objects/finders/PeriodicFinder.h \
    objects/finders/Voxel.h \
    objects/wrappers/AlignedStorage.h \
    objects/wrappers/Any.h \
    objects/wrappers/AutoPtr.h \
    objects/wrappers/ClonePtr.h \
    objects/wrappers/Expected.h \
    objects/wrappers/Finally.h \
    objects/wrappers/Flags.h \
    objects/utility/Iterators.h \
    objects/wrappers/LockingPtr.h \
    objects/wrappers/NonOwningPtr.h \
    objects/wrappers/ObserverPtr.h \
    objects/wrappers/Optional.h \
    objects/wrappers/Outcome.h \
    objects/wrappers/SharedPtr.h \
    objects/utility/Value.h \
    objects/wrappers/Variant.h \
    objects/wrappers/VectorizedArray.h \
    physics/Analytic.h \
    physics/Constants.h \
    physics/Damage.h \
    physics/Eos.h \
    physics/Integrals.h \
    physics/Rheology.h \
    physics/TimeFormat.h \
    post/Plot.h \
    quantities/Iterate.h \
    quantities/Quantity.h \
    quantities/QuantityHelpers.h \
    quantities/QuantityIds.h \
    quantities/Storage.h \
    sph/Diagnostics.h \
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
    system/Profiler.h \
    system/Settings.h \
    system/Statistics.h \
    system/Timer.h \
    tests/Setup.h \
    thread/AtomicFloat.h \
    thread/CheckFunction.h \
    thread/Pool.h \
    thread/ThreadLocal.h \
    timestepping/TimeStepCriterion.h \
    timestepping/TimeStepping.h \
    quantities/Particle.h \
    sph/solvers/AsymmetricSolver.h \
    objects/finders/KdTree.h \
    post/MarchingCubes.h \
    post/MeshFile.h \
    gravity/Moments.h \
    gravity/BruteForceGravity.h \
    objects/finders/BruteForceFinder.h \
    objects/wrappers/RawPtr.h \
    gravity/VoxelGravity.h \
    objects/objects/geometry/Sphere.h \
    objects/containers/List.h \
    tests/Approx.h \
    system/Process.h \
    objects/wrappers/Function.h \
    mpi/ProcessPool.h \
    mpi/Serializable.h \
    mpi/Mpi.h \
    post/Analysis.h \
    objects/utility/OperatorTemplate.h \
    objects/containers/Map.h \
    objects/wrappers/Interval.h \
    timestepping/ISolverr.h \
    quantities/IMaterial.h \
    gravity/IGravity.h \
    objects/finders/INeighbourFinder.h \
    run/IRun.h \
    sph/Materials.h \
    objects/geometry/Domain.h
