#!/bin/gnuplot 

set term pngcairo size 3000,1500
set output "plot.png"

unset colorbox
unset tics
set lmargin 0.2
set rmargin 0.2
set bmargin 0.2
set tmargin 0.2
set xrange [-1.e5:1.e5]
set yrange [-1.e5:1.e5]

set cbrange [1e4:1e5]
set logscale cb

set pointsize 2

set palette defined ( 0 0.0 0.02 0.09, \
                      0.25 0.4 0.106 0.38, \
                      0.5  0.78 0.18 0.38, \
                      0.75 0.91 0.56 0.81, \
                      1.0  0.29 0.69 0.93)
set multiplot layout 2,4

do for [i=1:8] {
    plot sprintf("result_%04d.txt", i) u 1:2:(0.001*$7):(sqrt($4*$4+$5*$5+$6*$6)) pt 7 ps var palette not
}


unset multiplot

set output

