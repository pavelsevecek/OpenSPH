TEMPLATE = lib
CONFIG += c++14 staticlib thread
CONFIG -= app_bundle qt

include(sharedLib.pro)

SOURCES += \
    common/Assert.cpp \
    gravity/AggregateSolver.cpp \
    gravity/BarnesHut.cpp \
    gravity/Galaxy.cpp \
    gravity/NBodySolver.cpp \
    gravity/cuda/CudaGravity.cpp \
    io/FileManager.cpp \
    io/FileSystem.cpp \
    io/Logger.cpp \
    io/Output.cpp \
    io/Path.cpp \
    math/Curve.cpp \
    math/Morton.cpp \
    math/SparseMatrix.cpp \
    math/rng/Rng.cpp \
    mpi/Mpi.cpp \
    objects/containers/String.cpp \
    objects/finders/Bvh.cpp \
    objects/finders/DynamicFinder.cpp \
    objects/finders/KdTree.cpp \
    objects/finders/NeighbourFinder.cpp \
    objects/finders/PeriodicFinder.cpp \
    objects/finders/UniformGrid.cpp \
    objects/geometry/Domain.cpp \
    objects/geometry/SymmetricTensor.cpp \
    objects/utility/Dynamic.cpp \
    objects/utility/StringUtils.cpp \
    objects/wrappers/Interval.cpp \
    physics/Damage.cpp \
    physics/Eos.cpp \
    physics/Functions.cpp \
    physics/Integrals.cpp \
    physics/Rheology.cpp \
    physics/TimeFormat.cpp \
    physics/Units.cpp \
    post/Analysis.cpp \
    post/MarchingCubes.cpp \
    post/MeshFile.cpp \
    post/Plot.cpp \
    post/StatisticTests.cpp \
    quantities/IMaterial.cpp \
    quantities/Particle.cpp \
    quantities/Quantity.cpp \
    quantities/QuantityIds.cpp \
    quantities/Storage.cpp \
    run/IRun.cpp \
    run/Node.cpp \
    run/SpecialEntries.cpp \
    run/VirtualSettings.cpp \
    sph/Diagnostics.cpp \
    sph/Material.cpp \
    sph/boundary/Boundary.cpp \
    sph/equations/Accumulated.cpp \
    sph/equations/Derivative.cpp \
    sph/equations/EquationTerm.cpp \
    sph/equations/Rotation.cpp \
    sph/equations/av/Stress.cpp \
    sph/handoff/Handoff.cpp \
    sph/initial/Distribution.cpp \
    sph/initial/Initial.cpp \
    sph/initial/MeshDomain.cpp \
    sph/solvers/AsymmetricSolver.cpp \
    sph/solvers/EnergyConservingSolver.cpp \
    sph/solvers/GradHSolver.cpp \
    sph/solvers/GravitySolver.cpp \
    sph/solvers/StandardSets.cpp \
    sph/solvers/StaticSolver.cpp \
    sph/solvers/SummationSolver.cpp \
    sph/solvers/SymmetricSolver.cpp \
    sph/solvers/cuda/CudaSolver.cpp \
    system/ArgsParser.cpp \
    system/Factory.cpp \
    system/Platform.cpp \
    system/Process.cpp \
    system/Profiler.cpp \
    system/Settings.cpp \
    system/Statistics.cpp \
    system/Timer.cpp \
    tests/Setup.cpp \
    thread/CheckFunction.cpp \
    thread/OpenMp.cpp \
    thread/Pool.cpp \
    thread/Scheduler.cpp \
    thread/Tbb.cpp \
    timestepping/TimeStepCriterion.cpp \
    timestepping/TimeStepping.cpp \
    io/LogWriter.cpp \
    sph/solvers/ElasticDeformationSolver.cpp \
    sph/solvers/DensityIndependentSolver.cpp \
    run/Worker.cpp \
    run/Config.cpp \
    run/workers/Presets.cpp \
    run/workers/SimulationWorkers.cpp \
    run/workers/GeometryWorkers.cpp \
    run/workers/InitialConditionWorkers.cpp \
    run/workers/MaterialWorkers.cpp \
    run/workers/IoWorkers.cpp \
    run/workers/ParticleWorkers.cpp

HEADERS += \
    Sph.h \
    common/Assert.h \
    common/ForwardDecl.h \
    common/Globals.h \
    common/Traits.h \
    gravity/AggregateSolver.h \
    gravity/BarnesHut.h \
    gravity/BruteForceGravity.h \
    gravity/CachedGravity.h \
    gravity/Collision.h \
    gravity/Galaxy.h \
    gravity/IGravity.h \
    gravity/Moments.h \
    gravity/NBodySolver.h \
    gravity/SphericalGravity.h \
    gravity/cuda/CudaGravity.h \
    gravity/cuda/DeviceMath.h \
    io/Column.h \
    io/FileManager.h \
    io/FileSystem.h \
    io/Logger.h \
    io/Output.h \
    io/Path.h \
    io/Serializer.h \
    io/Table.h \
    math/AffineMatrix.h \
    math/Curve.h \
    math/Functional.h \
    math/MathBasic.h \
    math/MathUtils.h \
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
    objects/containers/Grid.h \
    objects/containers/List.h \
    objects/containers/LookupMap.h \
    objects/containers/Queue.h \
    objects/containers/StaticArray.h \
    objects/containers/String.h \
    objects/containers/Tuple.h \
    objects/finders/AdaptiveGrid.h \
    objects/finders/BruteForceFinder.h \
    objects/finders/Bvh.h \
    objects/finders/DynamicFinder.h \
    objects/finders/KdTree.h \
    objects/finders/KdTree.inl.h \
    objects/finders/LinkedList.h \
    objects/finders/Linkedlist.h \
    objects/finders/NeighbourFinder.h \
    objects/finders/Octree.h \
    objects/finders/Order.h \
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
    objects/geometry/Triangle.h \
    objects/geometry/Vector.h \
    objects/utility/ArrayUtils.h \
    objects/utility/Dynamic.h \
    objects/utility/EnumMap.h \
    objects/utility/Iterator.h \
    objects/utility/IteratorAdapters.h \
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
    quantities/CompressedStorage.h \
    quantities/IMaterial.h \
    quantities/Iterate.h \
    quantities/Particle.h \
    quantities/Quantity.h \
    quantities/QuantityHelpers.h \
    quantities/QuantityIds.h \
    quantities/Storage.h \
    run/IRun.h \
    run/Node.h \
    run/Trigger.h \
    sph/Diagnostics.h \
    sph/Materials.h \
    sph/boundary/Boundary.h \
    sph/equations/Accumulated.h \
    sph/equations/DeltaSph.h \
    sph/equations/Derivative.h \
    sph/equations/DerivativeHelpers.h \
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
    sph/handoff/Handoff.h \
    sph/initial/Distribution.h \
    sph/initial/Initial.h \
    sph/initial/MeshDomain.h \
    sph/kernel/GravityKernel.h \
    sph/kernel/Interpolation.h \
    sph/kernel/Kernel.h \
    sph/solvers/AsymmetricSolver.h \
    sph/solvers/CollisionSolver.h \
    sph/solvers/DensityIndependentSolver.h \
    sph/solvers/EnergyConservingSolver.h \
    sph/solvers/EntropySolver.h \
    sph/solvers/GradHSolver.h \
    sph/solvers/GravitySolver.h \
    sph/solvers/StabilizationSolver.h \
    sph/solvers/StandardSets.h \
    sph/solvers/StaticSolver.h \
    sph/solvers/SummationSolver.h \
    sph/solvers/SymmetricSolver.h \
    sph/solvers/cuda/CudaSolver.h \
    system/ArgsParser.h \
    system/ArrayStats.h \
    system/Column.h \
    system/Crashpad.h \
    system/Element.h \
    system/Factory.h \
    system/Platform.h \
    system/Process.h \
    system/Profiler.h \
    system/RunCallbacks.h \
    system/Settings.h \
    system/Settings.impl.h \
    system/Statistics.h \
    system/Timer.h \
    tests/Approx.h \
    tests/Setup.h \
    thread/AtomicFloat.h \
    thread/CheckFunction.h \
    thread/OpenMp.h \
    thread/Pool.h \
    thread/Scheduler.h \
    thread/Tbb.h \
    thread/ThreadLocal.h \
    timestepping/ISolver.h \
    timestepping/ISolverr.h \
    timestepping/TimeStepCriterion.h \
    timestepping/TimeStepping.h \
    io/LogWriter.h \
    objects/finders/PeriodicFinder.h \
    sph/solvers/ElasticDeformationSolver.h \
    run/VirtualSettings.h \
    objects/containers/UnorderedMap.h \
    run/VirtualSettings.inl.h \
    run/Worker.h \
    run/Config.h \
    run/workers/Presets.h \
    run/workers/GeometryWorkers.h \
    run/workers/MaterialWorkers.h \
    run/workers/SimulationWorkers.h \
    run/workers/IoWorkers.h \
    run/workers/ParticleWorkers.h \
    run/workers/InitialConditionWorkers.h \
    run/workers/SpecialEntries.h \
    run/SpecialEntries.h

OTHER_FILES += \
    sph/solvers/cuda/CudaSolver.cu \
    gravity/cuda/CudaGravity.cu

CUDA_SOURCES += \
    sph/solvers/cuda/CudaSolver.cu \
    gravity/cuda/CudaGravity.cu

CUDA_DIR = /opt/cuda

INCLUDEPATH += $$CUDA_DIR/include

QMAKE_LIBDIR += $$CUDA_DIR/lib

NVCCFLAGS     = --compiler-options -fno-strict-aliasing -use_fast_math --ptxas-options=-v
#CUDA_INC = $$join(INCLUDEPATH,' -I','-I',' ')
CUDA_INC = -I/opt/cuda/include -I. -I.. -I/home/pavel/projects/astro/sph/src/lib

cuda.commands = $$CUDA_DIR/bin/nvcc -m64 -O3 -std c++14 -c $$NVCCFLAGS $$CUDA_INC ${QMAKE_FILE_NAME} -o ${QMAKE_FILE_OUT}

#-gencode arch=compute_37,code=sm_37

# nvcc error printout format ever so slightly different from gcc
# http://forums.nvidia.com/index.php?showtopic=171651

#cuda.dependency_type = TYPE_C # there was a typo here. Thanks workmate!
#cuda.depend_command = $$CUDA_DIR/bin/nvcc -O3 -M $$CUDA_INC $$NVCCFLAGS ${QMAKE_FILE_NAME}

cuda.input = CUDA_SOURCES
cuda.output = ${OBJECTS_DIR}${QMAKE_FILE_BASE}_cuda.o

QMAKE_EXTRA_COMPILERS += cuda

