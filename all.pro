TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = core \
          cli/launcher \
          cli/impact \
          cli/info \
          cli/problems \
          cli/sandbox \
          cli/ssftotxt \
          gui \
          gui/launcherGui \
          gui/sandboxGui \
          test \
          bench \
          post \
          examples/01_hello_asteroid \
          examples/02_death_star \
          examples/03_van_der_waals \
          examples/04_simple_collision \
          examples/05_fragmentation_reaccumulation \
          examples/06_heat_diffusion

run.depends = core
launcher.depends = core
impact.depends = core
info.depends = core
problems.depends = core
sandbox.depends = core
ssftotxt.depends = core
gui.depends = core
launcherGui.depends = core gui
sandboxGui.depends = core gui
test.depends = core
bench.depends = core
01_hello_asteroid.depends = core
02_death_star.depends = core
03_van_der_waals.depends = core
04_simple_collision = core
05_fragmentation_reaccumulation = core
06_heat_diffusion = core
