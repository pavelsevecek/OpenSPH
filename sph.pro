TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = lib \
          gui \
          cli/launcher \
          gui/launcherGui \
          gui/player

gui.depends = lib
launcher.depends = lib
launcherGui.depends = lib gui
player.depends = lib gui
