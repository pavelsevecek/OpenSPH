TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = core \
          bench \

bench.depends = core
