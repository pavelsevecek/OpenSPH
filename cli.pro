TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = core \
          cli/launcher \
          cli/batch

gui.depends = core
launcher.depends = core
batch.depends = core
