#!/bin/gnuplot 

set term pngcairo size 2500,1000
set output "plot.png"

unset colorbox
unset tics
set lmargin 0.2
set rmargin 0.2
set bmargin 0.2
set tmargin 0.2

#set logscale cb
set xrange [-6.e4:6.e4]
set yrange [-6.e4:6.e4]

set cbrange [1:100]

set palette defined ( 0 0.0 0.02 0.09, \
                      0.25 0.4 0.106 0.38, \
                      0.5  0.78 0.18 0.38, \
                      0.75 0.91 0.56 0.81, \
                      1.0  0.29 0.69 0.93)
set multiplot layout 2,5

do for [i=1:10] {
    plot sprintf("result_%04d.txt", i) u 1:2:(0.0005*$7):(sqrt($4*$4+$5*$5+$6*$6)) pt 7 ps var palette not
}


unset multiplot

set output

