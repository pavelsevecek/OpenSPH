TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          cli/ssftovdb \
          cli/meshtossf \

ssftovdb.depends = lib
meshtossf.depends = lib

