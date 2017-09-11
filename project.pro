TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          cli \
          cli/problems \
          gui \
          gui/collision \
          gui/rotation \
          test \
          bench \
    mpi \
    post \
    collision-cli

run.depends = lib
cli.depends = lib
problems.depends = lib
gui.depends = lib
collision.depends = lib gui
rotation.depends = lib gui
test.depends = lib
bench.depends = lib
