TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          gui \
          cli/launcher \
          gui/reacc

gui.depends = lib
launcher.depends = lib
reacc.depends = lib gui
