TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          run \
          problems \
          gui \
          test \
          bench

run.depends = lib
problems.depends = lib
gui.depends = lib
test.depends = lib
bench.depends = lib
