TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          test

test.depends = lib
