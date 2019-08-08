# OpenSPH

## Screenshots

<img src="http://sirrah.troja.mff.cuni.cz/~sevecek/sph/components.png" width="400">
<img src="http://sirrah.troja.mff.cuni.cz/~sevecek/sph/raytraced.png" width="400">
<img src="http://sirrah.troja.mff.cuni.cz/~sevecek/sph/sequence.png" width="400">
<img src="http://sirrah.troja.mff.cuni.cz/~sevecek/sph/cliff.png" width="400">
<img src="http://sirrah.troja.mff.cuni.cz/~sevecek/sph/editor.png" width="400">
<img src="http://sirrah.troja.mff.cuni.cz/~sevecek/sph/adjustui.png" width="400">

## About
OpenSPH is an integrator of hydrodynamic equations using SPH discretization in space, 
currently specialized on simulations of asteroid impacts. The code is being developed 
on Astronomical Institute of Charles University in Prague. It aims to provide a fast, 
versatile and easily extensible SPH solver utilizing modern CPU features (SSE/AVX 
instruction sets).

## Getting the code
The code can be downloaded from <a href="https://gitlab.com/sevecekp/sph/tree/devel">GitLab repository</a>.
Using git, you can clone the code with
```bash
git clone https://sevecekp@gitlab.com/sevecekp/sph.git
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
- QMake (tested with version 3.1)

Another optional dependencies of the code are:

- <a href="https://www.wxwidgets.org/">wxWidgets</a> (needed for graphical interface of the code)
- <a href="https://www.threadingbuildingblocks.org/">Intel Threading Building Blocks</a> (generally improves performance of the code)
- <a href="https://github.com/catchorg/Catch2">Catch2</a> (C++ unit testing framework)
- <a href="https://www.openvdb.org/">OpenVDB</a> (used for converting particles to volumetric data, usable by renderers)
- <a href="http://eigen.tuxfamily.org/index.php?title=Main_Page">Eigen</a> (for solving sparse systems)

The compilation should be as easy as
```bash
mkdir build_version
cd build_version
qmake CONFIG+=version ../sph.pro
make
```
where *version* can be one of:
- *release* - full-speed version of the code. This is the default option if no build version is specified
- *debug* - debugging build with no optimizations (SLOW)
- *assert* - build with all optimizations and additional sanity checks
- *profile* - full-speed build that measures durations of various segments of the code and print run statistics

Use different build directory for each version!

By default, OpenSPH uses a custom thread pool for parallelization. It is possible to use Intel TBB library 
instead, by adding use_tbb flag:
```bash
qmake CONFIG+=version CONFIG+=use_tbb ../sph.pro
```

The project sph.pro builds command-line launcher and the GUI application that allows to set up and run 
simulations, as well as view previously saved results.

To further build the code examples, run:
```bash
cd build_version
qmake CONFIG+=version ../examples.pro
make
```

## Windows binaries
Building the code on Windows is currently a bit difficult.
Consider using pre-built executables, uploaded to a 
<a href="https://www.dropbox.com/sh/qx4tdiai9epurzb/AAC7dx6mLyuWxFWjQQDYy22ua?dl=0">Dropbox</a>
folder.

## Running a basic impact simulation
A simulation can be started using a command-line launcher, located in cli/launcher directory.
When a launcher is started for the first time, it generates configuration files 
(with extension .cnf), which can be then modified to set up the simulation as needed.
Simulation can be also set up in the graphical application, using a node-based editor.
See category 'presets' on the right side of the editor for some basic simulations.

Default simulation uses the following:
- Equation of motion consists of a stress tensor divergence (SolidStressForce) and an artifial viscosity term (StandardAV)
- Density evolution is solved using continuity equation (ContinuityEquation)
- Hooke's law as a constitutive equation
- von Mises criterion to account for plastic yield (VonMisesRheology)
- Grady-Kipp model of fragmentation with scalar damage quantity (ScalarDamage)
- Tillotson equation of state (TillotsonEos)
- Adaptive smoothing length (AdaptiveSmoothingLength)
- Basalt material parameters

## Documentation
See [documentation](http://sirrah.troja.mff.cuni.cz/~sevecek/sph/docs/index.html)

## Examples of code usage
See [examples](http://sirrah.troja.mff.cuni.cz/~sevecek/sph/docs/Examples.html)

## Bug reports, ideas, question
Feel free to contact me at <a href="mailto:sevecek@sirrah.troja.mff.cuni.cz">sevecek@sirrah.troja.mff.cuni.cz</a>. 
Any feedback is highly appreciated.
