#!/bin/gnuplot

set term pngcairo enhanced size 800,600

#filename = "out_050"

set output sprintf("out/%s.png", filename)

set multiplot layout 2,2

set xrange [0:1]
set yrange [0:1.2]

set linestyle 1 lt 6 lc 1
set linestyle 2 lt 6 lc 2
set linestyle 3 lt 6 lc 3
set linestyle 4 lt 6 lc 4

plot sprintf("out/%s.txt", filename) u 1:7 t "density" w lp ls 1
plot sprintf("out/%s.txt", filename) u 1:4 t "velocity" w lp ls 2
plot sprintf("out/%s.txt", filename) u 1:10 t "pressure" w lp ls 3
plot sprintf("out/%s.txt", filename) u 1:8 t "energy" w lp ls 4

unset multiplot
set output
