TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          gui \
          cli/launcher \
          gui/launcherGui \

gui.depends = lib
launcher.depends = lib
launcherGui.depends = lib gui
