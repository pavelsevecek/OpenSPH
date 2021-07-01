# builds only unit tests

TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = core \
          test

test.depends = core
