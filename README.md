# OpenSPH  <a href="http://ascl.net/1911.003"><img src="https://img.shields.io/badge/ascl-1911.003-blue.svg?colorB=262255" alt="ascl:1911.003" /> </a><a href="https://gitlab.com/sevecekp/sph/commits/devel"><img src="https://gitlab.com/sevecekp/sph/badges/devel/pipeline.svg" /></a>


## Screenshots
See also the <a href="https://pavelsevecek.github.io/#two">render gallery</a>.

<img src="http://sirrah.troja.mff.cuni.cz/~sevecek/sph/components.png" width="400">
<img src="http://sirrah.troja.mff.cuni.cz/~sevecek/sph/raytraced.png" width="400">
<img src="http://sirrah.troja.mff.cuni.cz/~sevecek/sph/sequence.png" width="400">
<img src="http://sirrah.troja.mff.cuni.cz/~sevecek/sph/cliff.png" width="400">
<img src="http://sirrah.troja.mff.cuni.cz/~sevecek/sph/editor.png" width="400">
<img src="http://sirrah.troja.mff.cuni.cz/~sevecek/sph/adjustui.png" width="400">

## About
OpenSPH is a library and a GUI application for hydrodynamic and N-body simulations.
It uses SPH discretization in space and currently is specialized for simulations of 
asteroid impacts. The code is being developed on Astronomical Institute of Charles 
University. It aims to provide a fast, versatile and easily extensible SPH solver 
utilizing modern CPU features (SSE/AVX instruction sets).

## Quick start

### Windows

OpenSPH can be easily installed using the <a href="https://pavelsevecek.github.io/OpenSPH-0.4.0.msi">MSI installer</a>. 

### Debian and Ubuntu

The latest version can be downloaded as a <a href="https://gitlab.com/sevecekp/sph/-/jobs/artifacts/master/file/opensph_0.3.9-1.deb?job=build_package">Debian buster package</a>.
Install it via:
```
sudo dpkg -i opensph_0.3.9-1.deb
```
The package contains three executables:
- `opensph` - main program with graphical interface
- `opensph-cli` - command-line utility allowing to run simulations set up by `opensph`
- `opensph-info` - command-line utility for quick inspection of metadata of OpenSPH output files

### Arch Linux

OpenSPH package can be build from <a href="https://aur.archlinux.org/packages/opensph/">AUR</a>,
using e.g. trizen:
```
trizen -S opensph
```

### FreeBSD

The code is in the <a href="https://freebsd.pkgs.org/13/freebsd-amd64/OpenSPH-0.3.6.txz.html">FreeBSD ports collection</a> (big thanks to Yuri) and can be installed using:

```
pkg install OpenSPH
```

## Getting the source code
The code can be downloaded from <a href="https://gitlab.com/sevecekp/sph/tree/devel">GitLab repository</a>.
Using git, you can clone the code with
```bash
git clone https://gitlab.com/sevecekp/sph.git
cd sph
```
To get the latest (experimental) version, switch to develomnent branch using
```bash
git checkout devel
```

## Compilation
The code uses many c++14 features, so a reasonably new version of a C++ compiler is necessary.
The compilation has been tested on gcc 6.3.1 and clang 4.0.0.
Prerequisities of the code are:

- git (to get the code from the repository, skip if you already have the code)
- up-to-date version of gcc or clang compiler
- <a href="https://www.wxwidgets.org/">wxWidgets</a>, needed for graphical interface of the code
- cmake (version >= 3.5)

Another optional dependencies of the code are:

- <a href="https://www.threadingbuildingblocks.org/">Intel Threading Building Blocks</a> (version >= 2021.4) - generally improves performance of the code (enabled by `-DWITH_TBB=ON`)
- <a href="http://eigen.tuxfamily.org/index.php?title=Main_Page">Eigen</a> - provides additional methods for setting up initial conditions (enabled by `-DWITH_EIGEN=ON`)
- <a href="https://chaiscript.com/">ChaiScript</a> - allows to read and modify particle data from a script (enabled by `-DWITH_CHAISCRIPT=ON`)
- <a href="https://www.openvdb.org/">OpenVDB</a> (version >= 8.1) - used for converting particles to volumetric data, usable by renderers (enabled by `-DWITH_VDB=ON`)

The compilation should be as easy as
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

By default, OpenSPH uses a custom thread pool for parallelization. It is possible to use Intel TBB library 
instead, using:
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DWITH_TBB=ON ..
```

Alternatively, the code can be compiled using <a href="QMAKE.md">qmake build system</a>.

## Running a basic simulation
Simulations can be easily set up in the graphical application `opensph`, using a node-based editor.
To use one of the simulation templates, click 'Project -> New session' and select 
one of the presets. Alternatively, see the category 'presets' on the right side of the 
editor.
To start a simulation, either select 'Start run' from the 'Simulation' menu, or right-click 
any node and select 'Evaluate'.

For simulations of asteroid impacts, select the 'collision' or the 'fragmentation and reaccumulation'
preset. The default simulation uses the following:
- Equation of motion consists of a stress tensor divergence (SolidStressForce) and an artifial viscosity term (StandardAV)
- Density evolution is solved using continuity equation (ContinuityEquation)
- Hooke's law as a constitutive equation
- von Mises criterion to account for plastic yield (VonMisesRheology)
- Grady-Kipp model of fragmentation with scalar damage quantity (ScalarDamage)
- Tillotson equation of state (TillotsonEos)
- Adaptive smoothing length (AdaptiveSmoothingLength)
- Basalt material parameters

## Shearing-sheet support
OpenSPH now includes a native `shearing sheet` initial-condition node and a matching preset for local disk and
ring patches. The implementation follows the REBOUND shearing-sheet model:
- Hill-frame inertial terms in the local rotating Cartesian frame
- REBOUND-style `SEI` symplectic epicycle integrator for exact background shear/epicycle motion
- shear-periodic remapping in `x` with the REBOUND azimuthal offset and velocity jump
- REBOUND-style ghost boxes for collisions and gravity
- Bridges et al. restitution model and minimum-collision-velocity safeguard
- diagnostics for total mass, surface density, Toomre wavelength, and velocity dispersion

### GUI usage
In the node editor, create the `shearing sheet` initial-condition node or choose the `shearing_sheet`
preset. The node exposes the shearing-sheet box size, `Omega`, `G`, ghost-layer counts, particle-size
distribution, restitution model, timestep, softening, and solver hooks. The resulting particle data can be
connected directly to `N-body run` or `SPH run`.

### Headless usage
The shearing-sheet setup is serialized through standard job settings and run overrides. Any saved session or
project containing the `shearing sheet` node can be executed through the existing CLI/headless workflow
without additional scripting.

### Implementation notes
The runtime integration lives in the existing OpenSPH solvers and boundary infrastructure, not in a separate
standalone solver. N-body hard-sphere and soft-sphere paths both use the shearing-sheet remap and ghost-box
logic; SPH uses the same boundary mode and Hill-force hooks. Multi-stage timesteppers now propagate the
correct substep time into solver/boundary evaluation, so time-dependent ghost shifts remain consistent for
Leapfrog, modified-midpoint, Runge-Kutta, and SEI.

Current limitation: fixed softening is exact for brute-force gravity and for Barnes-Hut leaf interactions;
Barnes-Hut far-field node approximations still use a softened monopole rather than a fully softened higher
order multipole expansion, because OpenSPH's tree moments are derived for the Newtonian `1/r` kernel.

## Particle renderer
OpenSPH contains useful tools for visualization of particles. It allows rendering
individual spherical particles as well as rendering of isosurface, reconstructed 
from particles. The color palette can be specified from arbitrary state quantities,
making it easy to visualize particle velocities, internal energy, etc. 
The surface of bodies can be also textured with an arbitrary bitmap image.
![Impact animation](http://sirrah.troja.mff.cuni.cz/~sevecek/sph/render/anim.mp4)

Besides the orthographic and perspective projection, a fisheye 
camera for fulldome animations and a spherical 360° camera can be used.
![Fulldome camera](http://sirrah.troja.mff.cuni.cz/~sevecek/sph/render/fisheye.mp4)

## Examples of library usage
It is certainly possible to use to library without the GUI executable
and use the SPH or N-body solvers in other applications.
There [examples](http://sirrah.troja.mff.cuni.cz/~sevecek/sph/docs/Examples.html)
explain how to do that.

## Code documentation
See [documentation](http://sirrah.troja.mff.cuni.cz/~sevecek/sph/docs/index.html)

## Changelog
See [changelog](https://gitlab.com/sevecekp/sph/-/blob/devel/CHANGELOG.md)

## Bug reports, ideas, question
Feel free to contact me at <a href="mailto:sevecek@sirrah.troja.mff.cuni.cz">sevecek@sirrah.troja.mff.cuni.cz</a>. 
Any feedback is highly appreciated.
