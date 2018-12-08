TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          gui \
          cli/launcher \
          gui/reacc \
          gui/player

gui.depends = lib
launcher.depends = lib
reacc.depends = lib gui
player.depends = lib gui
