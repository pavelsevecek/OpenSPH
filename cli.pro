TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          cli/launcher \
          cli/batch

gui.depends = lib
launcher.depends = lib
batch.depends = lib
