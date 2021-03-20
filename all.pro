TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          cli/launcher \
          cli/impact \
          cli/info \
          cli/batch \
          cli/problems \
          cli/sandbox \
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

run.depends = lib
launcher.depends = lib
impact.depends = lib
info.depends = lib
batch.depends = lib
problems.depends = lib
sandbox.depends = lib
gui.depends = lib
launcherGui.depends = lib gui
sandboxGui.depends = lib gui
test.depends = lib
bench.depends = lib
01_hello_asteroid.depends = lib
02_death_star.depends = lib
03_van_der_waals.depends = lib
04_simple_collision = lib
05_fragmentation_reaccumulation = lib
06_heat_diffusion = lib
