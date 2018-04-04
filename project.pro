TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          cli \
          cli/problems \
          gui \
          gui/collision \
          gui/rotation \
          gui/nbody \
          gui/reacc \
          gui/player \
          test \
          bench \
    mpi \
    post \
    cli/collision-cli

run.depends = lib
cli.depends = lib
problems.depends = lib
gui.depends = lib
collision.depends = lib gui
player.depends = lib gui
rotation.depends = lib gui
nbody.depends = lib gui
reacc.depends = lib gui
test.depends = lib
bench.depends = lib
