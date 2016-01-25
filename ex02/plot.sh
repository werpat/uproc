#!/bin/bash
path=$1
show=$2

#TODO: i assume there are data and instruction l1 cache and they are both the same size and have the same line length
#   -> change this to correct behaviour for only taking data or unified cache into account
l1=0
if [ -e /sys/devices/system/cpu/cpu0/cache/index0 ]; then
    l1l=$(cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size)
    l1w=$(cat /sys/devices/system/cpu/cpu0/cache/index0/ways_of_associativity)
    l1s=$(cat /sys/devices/system/cpu/cpu0/cache/index0/number_of_sets)
    l1=$((l1w*l1s))
fi

l2=0
if [ -e /sys/devices/system/cpu/cpu0/cache/index2 ]; then
    l2l=$(cat /sys/devices/system/cpu/cpu0/cache/index2/coherency_line_size)
    l2w=$(cat /sys/devices/system/cpu/cpu0/cache/index2/ways_of_associativity)
    l2s=$(cat /sys/devices/system/cpu/cpu0/cache/index2/number_of_sets)
    l2=$((l2w*l2s))
fi

l3=0
if [ -e /sys/devices/system/cpu/cpu0/cache/index3 ]; then
    l3l=$(cat /sys/devices/system/cpu/cpu0/cache/index3/coherency_line_size)
    l3w=$(cat /sys/devices/system/cpu/cpu0/cache/index3/ways_of_associativity)
    l3s=$(cat /sys/devices/system/cpu/cpu0/cache/index3/number_of_sets)
    l3=$((l1+l2+l3w*l3s))
fi

cd ${path}
#NOTE: i assume all levels of caches have the same line length
gnuplot <<< "set terminal postscript enhanced color eps
    set datafile separator ','
    set output 'cache_line.eps'
    set xlabel 'stride [bytes]'
    set ylabel 'cycles per iteration'
    set arrow from ${l1l},graph(0,0) to ${l1l},graph(1,1) nohead lc rgb 'red'
    set xrange [:256]
    plot 'cache_line' using 1:2 with points lc rgb 'blue' pt 1"
gnuplot <<< "set terminal postscript enhanced color eps
    set datafile separator ','
    set output 'cache_size_all.eps'
    set xlabel 'memory blocks [64 bytes]'
    set ylabel 'cycles per iteration'
    set arrow from (${l1}),graph(0,0) to (${l1}),graph(1,1) nohead lc rgb 'red'
    set arrow from (${l2}),graph(0,0) to (${l2}),graph(1,1) nohead lc rgb 'red'
    set arrow from (${l3}),graph(0,0) to (${l3}),graph(1,1) nohead lc rgb 'red'
    plot 'cache_size' using 1:2 with points lc rgb 'blue' pt 1"

gnuplot <<< "set terminal postscript enhanced color eps
    set datafile separator ','
    set output 'cache_size_small.eps'
    set xlabel 'memory blocks [64 bytes]'
    set ylabel 'cycles per iteration'
    set xrange [:512*1024/64]
    set arrow from (${l1}),graph(0,0) to (${l1}),graph(1,1) nohead lc rgb 'red'
    set arrow from (${l2}),graph(0,0) to (${l2}),graph(1,1) nohead lc rgb 'red'
    set arrow from (${l3}),graph(0,0) to (${l3}),graph(1,1) nohead lc rgb 'red'
    plot 'cache_size' using 1:2 with points lc rgb 'blue' pt 1"

epstopdf cache_line.eps
rm cache_line.eps
epstopdf cache_size_all.eps
rm cache_size_all.eps
epstopdf cache_size_small.eps
rm cache_size_small.eps

if [[ "${show}" == *"l"* ]]; then
    xdg-open cache_line.pdf
fi
if [[ "${show}" == *"s"* ]]; then
    xdg-open cache_size_all.pdf
    xdg-open cache_size_small.pdf
fi

