TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS = core \
          examples/01_hello_asteroid \
          examples/02_death_star \
          examples/03_van_der_waals \
          examples/04_simple_collision \
          examples/05_fragmentation_reaccumulation \
          examples/06_heat_diffusion \

01_hello_asteroid.depends = core
02_death_star.depends = core
03_van_der_waals.depends = core
04_simple_collision = core
05_fragmentation_reaccumulation = core
06_heat_diffusion = core
