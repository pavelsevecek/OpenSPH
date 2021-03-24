## 2021-03-21
- added a reader for hdf5 (.h5) files
- generalized offset currently needed for OrthoCamera::unproject to work

## 2021-03-20
- hidden renderers that do not work properly from UI
- optimized particle renderer
- added shortcuts (ctrl + [1-9]) to switch between tabs
- XYZ axes are now drawn into the viewport
- project is added into the recent session list when saved
- batch run now properly converts enum values from string
- created opensph-info utility for quick inspection of .ssf files
- added experimental modes of the continuity equation
- added options for radii of spheres created by handoff
- parallelized EoS evaluation in SimpleSolver

## 2021-03-19
- proper enabling/disabling of merging criteria in UI

## 2021-03-17
- Path::extension() now returns the last file extension
- changed Path::replaceExtension() and Path::removeExtension() accordingly
- fixed SfdPlot throwing an exception when material has no DENSITY parameter

## 2021-03-12
- setting minimal density automatically
- increased default minimal density
- disabled wx asserts in release build
- added log verbosity parameter
- added separate parameter for the divergence criterion
- removing particles of already evaluated jobs

## 2021-03-09
- passing run statistics to dependent jobs
- added missing resize in BatchDialog
- added metadata for DeltaSPH density gradient
- cloning node hierarchy when run starts
- fixed CLI launcher

## 2021-03-08
 - fixed ContinuityEquation erasing other additions to density derivative

## 2021-03-01
 - removed heating due to AV from AC

## 2021-02-13
 - removed deprecated header

## 2021-02-13
 - fixed XSph changing smoothing lengths

## 2021-02-10
 - AsymmetricSolver: optimized search radius
 - updated test.pro

## 2021-01-08
 - fixed sample distribution UI entry
 - added new equilibrium solver
 - added Maclaurin spheroid domain

## 2020-12-21
 - added divergence-based timestep criterion
 - tweaked accretion palette
 - fixed enablers for material parameters
 - showing number of particles for all particle jobs
 - fixed parametrized spiralling particle count and radii
 - Disabling auto-zoom on mouse-wheel event

## 2020-12-20
 - fixed compilation with disabled ChaiScript
 - added period and one-shot parameter to script term
 - new color palettes
 - added simple soft-sphere solver 
 - renamed NBodySolver to HardSphereSolver

## 2020-12-19
 - fixed clamping in PaletteDialog
 - fixed fov of ortho camera
 - stabilization resets damage to its initial value rather than zero,
 - automatic region count in StratifiedDistribution,
 - regeneration of acoustric energy in acoustic fluidization,

## 2020-06-15
 - fixed int-uint comparisons
 - fixed implicit double-float conversions

## 2020-04-17
 - fixing unit tests
 - added anisotropic kernels to MC

## 2020-04-15
 - generalized SPH interpolant

## 2020-02-11
 - added option to select BRDF
 - refactored camera interface

## 2020-02-09
 - added simple solver
 - fixed HashMapFinder, added tests
 - added CenterParticlesJob

## 2020-01-19
 - added HashMapFinder
 - using hash map to determine search radius
 - removed DynamicFinder
 - implemented MeshDomain::addGhosts
 - added interval controls to UI
 - option to raytrace spheres instead of isosurfac
 - added smoothing length controls into UI

## 2020-01-17
 - reverted anaglyph changes
 - reverted camera changes
 - added GaussianRandomSphere domain
 - added validation tests (Sod, Sedov, Nakamura, ...)
 - renamed workers to jobs
 - added artificial conductivity
 - added XSPH and delta-SPH into settings
 - added parametrized spiraling distribution

## 2020-01-13
 - added min particle count to ImpactorIc

## 2020-01-11
 - updated Sod solution
 - added Sod analytical solution
 - added dimensions to SymmetricSolver as template parameter

## 2020-01-10
 - temporarily removed failing test

## 2020-01-09
 - added missing member variable
 - changed progressbar color on run end
 - improved mesh extraction
 - fixed HDE
 - drawing activated node as bold
 - fixed creating recent project cache

## 2019-12-23
 - relaxed KS test
 - fixed float conversions in lib
 - added stress AV parameters to UI
 - removed multiplier in SummedDensityColorizer

## 2019-11-19
 - added chaiscript worker
 - added symmetric boundary condition
 - added camera autosetup
 - added single particle worker
 - added PropagateConst wrapper
 - fixed compilation with gcc-7

## 2019-11-03
 - added basic colormapping curve
 - added sandboxGui project

## 2019-10-28
 - fixed UniqueNameManager tests
 - removed missing files from .pro

## 2019-10-27
 - added Batch runner dialog
 - added OpenMP-based scheduler
 - added particles-to-components handoff worker
 - unified camera parameters
 - fixed compilation with GCC
 - added customization controls to GridPage

## 2019-10-19
 - added fisheye and spherical projection
 - added cli executable for running .sph file
 - added surface tension to getStandardEquations and GUI
 - set particle radius of loaded SPH files to 0.35
 - added number of particles to fragment parameters

## 2019-10-01
 - adding tooltips to workers and settings entries

## 2019-09-27
 - added Weibull parameters to UI

## 2019-09-11
 - improving elastic solver
 - added Perlin noise worker
 - added ortho camera parameters to UI
 - fixed unit tests

## 2019-09-06
 - added contour renderer
 - fixed incorrect particle duplication

## 2019-09-05
 - fixed stress tensor being integrated without rheology

## 2019-09-04
 - changed PeriodicBoundary to use ghosts instead of modified finder
 - refactored connecting of entries
 - added KillEscapersBoundary
 - automatically updating filename extensions based on file type
 - generalized PeriodicFinder to other dimensions
 - fixed compilation of tests
 - added curve controls
 - extended VirtualSetting by ExtraEntry,
 - setting up PeriodicFinder when PeriodicBoundary is used,
 - added quantity profiles to analysis menu

## 2019-08-08
 - moved analysis to main menu

## 2019-07-28
 - fixing CI script
 - trying clang from main repo
 - replaced trusty with xenial
 - removed clang install
 - added grid with fragment parameters
 - added batch runner
 - generalized differentiated body

## 2019-06-22
 - added galaxy collision preset
 - added galaxy problem
 - fixed scheduler initialization in problems
 - measuring runtime of problems

## 2019-06-19
 - removed Invoker from timestepping

## 2019-06-15
 - fixed tests

## 2019-06-12
 - added basic acoustic fluidization model
 - fixing cratering preset
 - fixed compilation of tests

## 2019-06-10
 - added cratering preset
 - added missing boundary conditions to StabilizationSolver
 - fixed resetting of cutoff
 - added checkbox for displaying ghosts
 - added sound speed colorizer

## 2019-06-02
 - fixed galaxy generation
 - added gravity constant parameter to settings
 - added other galaxy parameters to UI
 - fixed selecting particle
 - added basic ICs for galaxies
 - added emission to raytracer
 - added proper tent filtering to raytracer
 - fixed exception when using SUM_ONLY_UNDAMAGED without stress tensor

## 2019-05-31
 - removed tbbmalloc_debug
 - added INSTALLS variable to qmake projects
 - fixed compilation with Eigen
 - replaced math.h header with cmath
 - added node for creating OpenVDB grids
 - added cache for recent sessions
 - fixed infinite loop when loading non-existent files
 - button Refresh correctly refreshes the image when checkbox "refresh on timestep" is unchecked
 - fixed closing plot pages
 - fixed parallelization of raytracer

## 2019-05-29
 - added mode to render file sequence
 - added transparency to UI and render worker
 - changed Project to singleton
 - added light controls to render worker
 - moved Movie rendering out of main thread
 - fixed near clipping of perspective camera
 - added overplot to SFD plots
 - reworked granularity of schedulers
 - fixed scheduler type and granularity not being taken into account
 - added about dialog
 - added more parameters for SPH solver
 - fixed cloning of LoadFileWorker
 - fixed emplacer not working if input does not store flags,
 - initializing UI checkboxes to correct values,
 - fixed raytracer controls not being enabled,
 - added random seed into component colorizer UI

## 2019-05-26
 - replaced float spinners with custom float text control
 - fixed Storage test
 - increased timestep for example 4
 - changed palette of the correction tensor

## 2019-05-24
 - fixing warnings & linker errors
 - updated readme
 - cumulative update from run-nodes branch



Older versions are only documented in git commit messages.
