## 2021-08-12
- optimized preview render refresh when palette changes
- volumetric renderer: made emission independent of smoothing lengths

## 2021-08-11
- added special point-mass particles ("attractors"), created via 'create single particle' node
- added toroidal domain
- added node that sets up Keplerian velocities
- generalized N-body ICs, it can now use any domain
- added asteroid belt to the 'Solar system' preset
- added simple bloom effect

## 2021-08-07
- suppressing low-frequency noise in images rendered with volumetric renderer

## 2021-08-06
- added OpenVDB option to cmake build system
- added velocity grid to VDB output files

## 2021-07-31
- added Delaunay triangulation of points
- added an option to extract surface mesh as the alpha-shape
- mesh resolution is now relative to the median particle radius
- fixed path type in 'Save mesh' node
- added optimization tags to Flat containers

## 2021-07-25
- added "render page" showing current status of an Animation job
- added render preview to Animation job
- added new node type - image node
- removed multiple quantities from Animation job (only one can be selected)
- removed 'orbit' option from Animation job
- added notification callbacks to JobNode
- reworked Movie object, removed it from Controller
- removed palette from rendered image, palette is now in separate panel
- fixed VolumeRenderer with transparent background
- properly adjusting FoV when OrthoCamera is resized
- setting default particle radius of SPH .scf files to 0.35
- registered format function to use in Chaiscript
- changed 'batch' simulation from JobNode to custom INode implementation
- added spherical camera job
- implemented functions setPosition, setTarget and project for panoramatic cameras

## 2021-07-20
- improved polytrope IC
- optimized refresh when render parameters change
- Raymarcher: fixed shading artifacts when shadows are enabled
- optional frame interpolation in animation node

## 2021-07-18
- optimized search radius in AsymmetricSolver

## 2021-07-17
- added a simple 'dust' rheology

## 2021-07-16
- added an option to enable shadows in raymarcher
- removed experimental renderers from UI

## 2021-07-15
- enabled the 'create camera node' for loaded states

## 2021-07-13
- automatic computation of particle radii in volumetric renderer
- added absorption to volumetric renderer
- added compression factor to logarithmic colormapper
- properly validating palette ranges
- added progress reporting to galaxy IC (parts only)

## 2021-07-11
- implemented softening kernels for other SPH kernels
- added a node that provides setup of orbiting bodies
- included surface gravity in GravityColorizer
- automatically selecting the file type in SaveFileJob
- Bvh: replaced std::set with output iterator

## 2021-07-10
- added basic polytrope model
- fixed limited range of float entries in property grid
- made mass palette logarithmic
- added flag renumbering in multi-join

## 2021-07-09
- optimized NBodyIc
- optimized GalaxyIc
- fixed crash when closing paused simulation

## 2021-07-08
- added GUI settings dialog
- added option to set the default colorizer
- removed strict type safety from ExtendedEnum
- added angular histogram
- fixed XYZ axes being mirrored
- fixed camera tracking and camera velocity
- added camera orbit to camera jobs
- added units to camera parameters
- switched the default colorizer to velocity
- properly selecting the default colorizer
- fixed bugged Perlin noise

## 2021-07-06
- explicitly setting path type for all path entries
- added validator to entries
- using validator to limit available output types
- unified available input/output formats
- removed splitter adjustment when selection a node
- fixed crash in CompressedOutput when run time is not specified

## 2021-07-03
- detached cameras from Animation node
- added volumetric renderer to UI
- fixed Any class
- deduplicated raytracing code
- removed "force grayscale" from UI
- added menu entry for creating camera from current view
- removed "cache" node

## 2021-07-01
- added basic cmake buildsystem
- added allocators to Array and List
- added isothermal sphere IC
- made parameters of SoftSphereSolver dimensionless

## 2021-06-26
- added particle densities to Chaiscript functions
- added volumetric renderer (job-only)
- added colormapping options

## 2021-06-23
- added a node for removing damaged/expanded particles
- fixed radius colorizer missing in the combobox

## 2021-06-19
- added a way to start simulations from chaiscript
- creating AV from setting in SummationSolver
- fixed closing the window despite 'cancel' being pressed
- added an alternative signal speed to ArtificialConductivity

## 2021-06-03
- added "create session" dialog
- throwing exception in HalfSpaceDomain::getVolume and getSurfaceArea 

## 2021-05-31
- palettes are now saved directly into the .sph file
- replaced hashes with std::type_index in EnumWrapper

## 2021-05-30
- replaced /usr with $$PREFIX in .pro files
- fixed linking of OpenVDB
- fixed segfault when opening a new session
- fixed button positioning in "Select Run" dialog
- fixed potential deadlock when closing the run

## 2021-05-29
- added run select dialog
- properly closing run controllers
- disabled Analysis menu in node page
- made compilable on FreeBSD 13
- fixed incorrect interpolation of GravityLutKernel at the last close bin

## 2021-04-27
- added planar UV mapping

## 2021-04-11
- moved textures to material

## 2021-04-08
- showing the confirmation dialog when closing the window
- sorted the colorizers
- fixed simulation failing when a colorizer that cannot be used is selected

## 2021-04-07
- customizable spacing of output times
- skipping divergence criterion if there is no divergence in the storage
- particles of heterogeneous body are now connected (flag-wise)
- added spin to the transform job
- added a regression test for rotation

## 2021-03-28
- added MpcorpInput
- added camera velocity into AnimationJob
- added gravity colorizer
- fixed rotation of ortho camera
- perspective camera no longer clamps particle radius to 1

## 2021-03-27
- significant optimiazations of collisions
- added persistent indices to N-body runs
- using persistent indices for particle tracking
- added cppcheck and flawfinder to CI

## 2021-03-26
- optimized collision handling
- added few auxiliary functions to Array, FlatSet and FlatMap
- fixed loading string parameters in CLI launcher
- tweaking CI regression testing

## 2021-03-25
- added planetesimal merge preset
- properly enabling parameters in output category
- added Tillotson's parameters to material jobs
- optimizations of HardSphereSolver for large number of collisions

## 2021-03-24
- added compare node
- added a simple regression test to CI
- fixed batch run causing crashes
- tweaked presets
- added multi-join operator for merging multiple particle sources
- added option to disable collisions in N-body run
- added accretion preset
- added Solar System preset
- fixed missing parameter in transform node

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

