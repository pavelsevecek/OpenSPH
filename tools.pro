TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          cli/ssftovdb \
          cli/meshtossf \
          cli/ssftotxt \

ssftovdb.depends = lib
meshtossf.depends = lib
ssftotxt.depends = lib

