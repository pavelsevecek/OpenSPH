TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = core \
          gui \
          cli/ssftovdb \
          cli/meshtossf \
          cli/ssftotxt \
          cli/ssftoscf \
          cli/ssftoout \
          gui/ssftopng

gui.depends = core
ssftovdb.depends = core
meshtossf.depends = core
ssftotxt.depends = core
ssftoscf.depends = core
ssftoout.depends = core
ssftopng.depends = core gui

