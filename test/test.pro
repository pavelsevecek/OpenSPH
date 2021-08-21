TEMPLATE = app
CONFIG += c++14 thread silent
CONFIG -= app_bundle
CONFIG -= qt

LIBS += ../core/libcore.a

CONFIG(devel) {
    DEFINES += SPH_CONFIG_SET
}

DEPENDPATH += . ../core
LIBS += ../core/libcore.a
PRE_TARGETDEPS += ../core/libcore.a

include(../core/sharedCore.pro)

INCLUDEPATH += ../core $$PREFIX/include/catch2

SOURCES += \
    ../core/common/test/Traits.cpp \
    ../core/gravity/test/BarnesHut.cpp \
    ../core/gravity/test/BruteForceGravity.cpp \
    ../core/gravity/test/Moments.cpp \
    ../core/gravity/test/NBodySolver.cpp \
    ../core/io/test/FileManager.cpp \
    ../core/io/test/FileSystem.cpp \
    ../core/io/test/Logger.cpp \
    ../core/io/test/Output.cpp \
    ../core/io/test/Path.cpp \
    ../core/io/test/Serializer.cpp \
    ../core/math/rng/test/Rng.cpp \
    ../core/math/rng/test/VectorRng.cpp \
    ../core/math/test/AffineMatrix.cpp \
    ../core/math/test/Math.cpp \
    ../core/math/test/Means.cpp \
    ../core/math/test/Morton.cpp \
    ../core/math/test/Quat.cpp \
    ../core/math/test/SparseMatrix.cpp \
    ../core/objects/containers/test/Allocators.cpp \
    ../core/objects/containers/test/Array.cpp \
    ../core/objects/containers/test/ArrayRef.cpp \
    ../core/objects/containers/test/ArrayView.cpp \
    ../core/objects/containers/test/CircularArray.cpp \
    ../core/objects/containers/test/CallbackSet.cpp \
    ../core/objects/containers/test/FlatMap.cpp \
    ../core/objects/containers/test/FlatSet.cpp \
    ../core/objects/containers/test/Grid.cpp \
    ../core/objects/containers/test/List.cpp \
    ../core/objects/containers/test/StaticArray.cpp \
    ../core/objects/containers/test/String.cpp \
    ../core/objects/containers/test/Queue.cpp \
    ../core/objects/containers/test/Tuple.cpp \
    ../core/objects/finders/test/BruteForceFinder.cpp \
    ../core/objects/finders/test/Finders.cpp \
    ../core/objects/finders/test/IncrementalFinder.cpp \
    ../core/objects/finders/test/Order.cpp \
    ../core/objects/finders/test/Bvh.cpp \
    ../core/objects/geometry/test/AntisymmetricTensor.cpp \
    ../core/objects/geometry/test/Box.cpp \
    ../core/objects/geometry/test/Domain.cpp \
    ../core/objects/geometry/test/Indices.cpp \
    ../core/objects/geometry/test/Multipole.cpp \
    ../core/objects/geometry/test/Sphere.cpp \
    ../core/objects/geometry/test/Triangle.cpp \
    ../core/objects/geometry/test/SymmetricTensor.cpp \
    ../core/objects/geometry/test/Tensor.cpp \
    ../core/objects/geometry/test/TracelessTensor.cpp \
    ../core/objects/geometry/test/Vector.cpp \
    ../core/objects/geometry/test/Plane.cpp \
    ../core/objects/geometry/test/Delaunay.cpp \
    ../core/objects/utility/test/Algorithm.cpp \
    ../core/objects/utility/test/Dynamic.cpp \
    ../core/objects/utility/test/Iterators.cpp \
    ../core/objects/utility/test/OperatorTemplate.cpp \
    ../core/objects/wrappers/test/AlignedStorage.cpp \
    ../core/objects/wrappers/test/Any.cpp \
    ../core/objects/wrappers/test/AutoPtr.cpp \
    ../core/objects/wrappers/test/ClonePtr.cpp \
    ../core/objects/wrappers/test/Expected.cpp \
    ../core/objects/wrappers/test/ExtendedEnum.cpp \
    ../core/objects/wrappers/test/Finally.cpp \
    ../core/objects/wrappers/test/Flags.cpp \
    ../core/objects/wrappers/test/Function.cpp \
    ../core/objects/wrappers/test/Interval.cpp \
    ../core/objects/wrappers/test/LockingPtr.cpp \
    ../core/objects/wrappers/test/Lut.cpp \
    ../core/objects/wrappers/test/Optional.cpp \
    ../core/objects/wrappers/test/Outcome.cpp \
    ../core/objects/wrappers/test/SharedPtr.cpp \
    ../core/objects/wrappers/test/SharedToken.cpp \
    ../core/objects/wrappers/test/Variant.cpp \
    ../core/objects/wrappers/test/PropagateConst.cpp \
    ../core/physics/test/Damage.cpp \
    ../core/physics/test/Eos.cpp \
    ../core/physics/test/Integrals.cpp \
    ../core/physics/test/TimeFormat.cpp \
    ../core/physics/test/Yielding.cpp \
    ../core/post/test/Analysis.cpp \
    ../core/post/test/TwoBody.cpp \
    ../core/post/test/MarchingCubes.cpp \
    ../core/post/test/MeshFile.cpp \
    ../core/post/test/Point.cpp \
    ../core/post/test/StatisticTests.cpp \
    ../core/quantities/test/IMaterial.cpp \
    ../core/quantities/test/Iterate.cpp \
    ../core/quantities/test/Particle.cpp \
    ../core/quantities/test/Attractor.cpp \
    ../core/quantities/test/Quantity.cpp \
    ../core/quantities/test/QuantityHelpers.cpp \
    ../core/quantities/test/Storage.cpp \
    ../core/quantities/test/Utility.cpp \
    ../core/run/test/Config.cpp \
    ../core/run/test/Jobs.cpp \
    ../core/sph/boundary/test/Boundary.cpp \
    ../core/sph/equations/av/test/AV.cpp \
    ../core/sph/equations/av/test/Balsara.cpp \
    ../core/sph/equations/av/test/MorrisMonaghan.cpp \
    ../core/sph/equations/av/test/Stress.cpp \
    ../core/sph/equations/test/Accumulated.cpp \
    ../core/sph/equations/test/Derivative.cpp \
    ../core/sph/equations/test/EquationTerm.cpp \
    ../core/sph/equations/test/Friction.cpp \
    ../core/sph/equations/test/Heat.cpp \
    ../core/sph/equations/test/Potentials.cpp \
    ../core/sph/equations/test/XSph.cpp \
    ../core/sph/initial/test/Distribution.cpp \
    ../core/sph/initial/test/Initial.cpp \
    ../core/sph/initial/test/Stellar.cpp \
    ../core/sph/kernel/test/GravityKernel.cpp \
    ../core/sph/kernel/test/Interpolation.cpp \
    ../core/sph/kernel/test/Kernel.cpp \
    ../core/sph/solvers/test/EquilbriumSolver.cpp \
    ../core/sph/solvers/test/GravitySolver.cpp \
    ../core/sph/solvers/test/Impact.cpp \
    ../core/sph/solvers/test/Solvers.cpp \
    ../core/sph/solvers/test/StandardSets.cpp \
    ../core/sph/solvers/test/AsymmetricSolver.cpp \
    ../core/sph/test/Diagnostics.cpp \
    ../core/system/test/ArgsParser.cpp \
    ../core/system/test/ArrayStats.cpp \
    ../core/system/test/Process.cpp \
    ../core/system/test/Profiler.cpp \
    ../core/system/test/Settings.cpp \
    ../core/system/test/Statistics.cpp \
    ../core/system/test/Timer.cpp \
    ../core/thread/test/AtomicFloat.cpp \
    ../core/thread/test/CheckFunction.cpp \
    ../core/thread/test/Pool.cpp \
    ../core/timestepping/test/TimeStepCriterion.cpp \
    ../core/timestepping/test/TimeStepping.cpp \
    main.cpp \
    utils/Approx.cpp \
    utils/SequenceTest.cpp \
    utils/Utils.cpp \
    ../core/sph/equations/test/Fluids.cpp \
    ../core/physics/test/Units.cpp \
    ../core/math/test/Functional.cpp \
    ../core/physics/test/Functions.cpp \
    ../core/sph/solvers/test/DensityIndependentSolver.cpp \
    ../core/thread/test/Scheduler.cpp \
    ../core/sph/solvers/test/EnergyConservingSolver.cpp \
    ../core/gravity/test/CachedGravity.cpp \
    ../core/run/test/IRun.cpp \
    ../core/system/test/Crashpad.cpp \
    ../core/objects/finders/test/PeriodicFinder.cpp \
    ../core/run/test/Node.cpp

HEADERS += \
    utils/Utils.h \
    utils/RecordType.h \
    utils/SequenceTest.h \
    utils/Config.h
