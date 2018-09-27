TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          cli \
          cli/launcher \
          cli/problems \
          cli/ssftovdb \
          gui \
          gui/collision \
          gui/rotation \
          gui/nbody \
          gui/reacc \
          gui/player \
          examples/01_hello_asteroid \
          examples/02_death_star \
          examples/03_van_der_waals \
          examples/04_simple_collision \
          examples/05_fragmentation_reaccumulation \
          test \
          bench \
          post

run.depends = lib
cli.depends = lib
launcher.depends = lib
problems.depends = lib
ssftovdb.depends = lib
gui.depends = lib
collision.depends = lib gui
player.depends = lib gui
rotation.depends = lib gui
nbody.depends = lib gui
reacc.depends = lib gui
test.depends = lib
bench.depends = lib
01_hello_asteroid.depends = lib
02_death_star.depends = lib
03_van_der_waals.depends = lib
04_simple_collision = lib
05_fragmentation_reaccumulation = lib
