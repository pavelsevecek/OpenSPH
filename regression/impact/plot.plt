#!/bin/gnuplot 

set term pngcairo size 1500,550
set output "plot.png"

unset colorbox
unset tics
set lmargin 0.2
set rmargin 0.2
set bmargin 0.2
set xrange [-1.2e3:1.2e3]
set yrange [-1.2e3:1.2e3]

set cbrange [0.1:100]
set logscale cb

set pointsize 2

set multiplot layout 1,3

set title "velocity"
plot "result.txt" u (abs($3)<100 ? $1 : 1./0.):2:(sqrt($4*$4+$5*$5+$6*$6)) pt 7 palette not

set cbrange [0.01:1000]
set palette rgb 21,22,23
set title "specific internal energy"
plot "result.txt" u (abs($3)<100 ? $1 : 1./0.):2:8 pt 7 palette not

unset logscale 
set palette gray
set cbrange [0:1.2]
set title "damage"
plot "result.txt" u (abs($3)<100 ? $1 : 1./0.):2:9 pt 7 palette not


unset multiplot

set output

