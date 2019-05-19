# OpenSPH

Interactive visualization
<img src="http://sirrah.troja.mff.cuni.cz/~sevecek/sph/snapshot.png" width="450">

Node-based setup of the simulation
<img src="http://sirrah.troja.mff.cuni.cz/~sevecek/sph/editor.png" width="450">

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
git checkout devel
```

## Compilation 
The code uses many c++14 features and will migrate to c++17 when it gets implemented 
by mainstream compilers. The compilation has been tested on gcc 6.3.1 and clang 4.0.0.
Prerequisities of the code are:

- git (to get the code from the repository, skip if you already have the code)
- up-to-date version of gcc or clang compiler
- QMake (tested with version 3.1)
- <a href="http://eigen.tuxfamily.org/index.php?title=Main_Page">Eigen</a> (for solving sparse systems)

Another optional dependencies of the code are:

- <a href="https://www.wxwidgets.org/">wxWidgets</a> (needed for graphical interface of the code)
- <a href="https://github.com/philsquared/Catch">Catch</a> (C++ unit testing framework)
- <a href="https://github.com/google/benchmark">Google Benchmark</a> (comparing perfomance of different solvers, code settings, etc.)

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
Use different build directory for each version.

## Running a basic impact simulation
The code can be executed with default settings, in which case it will use the following:
- Equation of motion consists of a stress tensor divergence (SolidStressForce) and an artifial viscosity term (StandardAV)
- Density evolution is solved using continuity equation (ContinuityEquation)
- Hooke's law as a constitutive equation
- von Mises criterion to account for plastic yield (VonMisesRheology)
- Grady-Kipp model of fragmentation with scalar damage quantity (ScalarDamage)
- Tillotson equation of state (TillotsonEos)
- Adaptive smoothing length (AdaptiveSmoothingLength)
- Basalt material parameters

Only thing that needs to be set up by the user is the initial conditions of the simulation.
For the impact experiment, this can be easily done with InitialConditions object.

See file cli/main.cpp for an example of a simple impact simulation.

## Documentation
See [documentation](http://sirrah.troja.mff.cuni.cz/~sevecek/sph/docs/index.html)

## Examples of the code usage
See [examples](http://sirrah.troja.mff.cuni.cz/~sevecek/sph/docs/Examples.html)

## Bug reports, ideas, question
Feel free to contact me at <a href="mailto:sevecek@sirrah.troja.mff.cuni.cz">sevecek@sirrah.troja.mff.cuni.cz</a>. 
Any feedback is highly appreciated.
