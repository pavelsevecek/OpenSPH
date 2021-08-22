#!/bin/bash

for f in `ls perf-*.csv`
do
    sha=${f:5:8}
    timestamp=`git show -s --format=%ci $sha`
    echo "Processing SHA $sha (timestamp $timestamp)"
    cat $f | while read line
    do
        name=`echo $line | awk -F',' '{print $1}'`
        fname="${name// /_}.txt"
        duration=`echo $line | awk -F',' '{print $4}'`
        echo "Test $fname took $duration"
        echo "\"$timestamp\" $duration" >> $fname
    done
done

for f in `ls *.txt`
do
   sort -o $f $f
   plotfile="${f%.*}.pdf"
   testname="${f//_/ }"
   testname="${testname%.*}"
   echo "Plotting $testname to $plotfile"
   gnuplot -e "\
     set term pdfcairo size 12,8;\
     set output '$plotfile';\
     set bmargin 13;\
     set title '$testname';\
     set xtics rotate by -60;\
     plot '$f' using 2:xticlabels(1) w lp pt 7 not;\
     set output"
done
