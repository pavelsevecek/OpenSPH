TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          run \
          problems \
          gui \
          test
      

run.depends = lib
problems.depends = lib
gui.depends = lib
test.depends = lib
