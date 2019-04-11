TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          gui \
          cli/ssftovdb \
          cli/meshtossf \
          cli/ssftotxt \
          cli/ssftoscf \
          cli/ssftoout \
          gui/ssftopng

gui.depends = lib
ssftovdb.depends = lib
meshtossf.depends = lib
ssftotxt.depends = lib
ssftoscf.depends = lib
ssftoout.depends = lib
ssftopng.depends = lib gui

