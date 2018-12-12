TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          cli/launcher \
          cli/problems \
          gui \
          gui/collision \
          gui/launcherGui \
          gui/nbody \
          gui/rubblepile \
          gui/player \
          test \
          bench \
          post

run.depends = lib
launcher.depends = lib
problems.depends = lib
gui.depends = lib
collision.depends = lib gui
player.depends = lib gui
nbody.depends = lib gui
launcherGui.depends = lib gui
rubblepile.depends = lib gui
test.depends = lib
bench.depends = lib
