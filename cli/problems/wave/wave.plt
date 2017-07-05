#!/bin/gnuplot

set term pngcairo enhanced size 500,800

#filename = "out_050"

set output sprintf("%s.png", filename)

set xrange [-3:3]
set yrange [-11:11]
set cbrange [800:1200]

set linestyle 1 lt 6 lc 1
set linestyle 2 lt 6 lc 2
set linestyle 3 lt 6 lc 3
set linestyle 4 lt 6 lc 4


plot sprintf("%s.txt", filename) u (abs($2) < 0.1 ? $1 : 1./0.):3:7 t "density" w p pt 7 ps 0.4 lc palette

set output
