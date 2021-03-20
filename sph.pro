TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          gui \
          cli/launcher \
          cli/info \
          gui/launcherGui \

gui.depends = lib
launcher.depends = lib
info.depends = lib
launcherGui.depends = lib gui
