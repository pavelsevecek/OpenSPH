TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = core \
          gui \
          cli/launcher \
          cli/info \
          gui/launcherGui \

gui.depends = core
launcher.depends = core
info.depends = core
launcherGui.depends = core gui
