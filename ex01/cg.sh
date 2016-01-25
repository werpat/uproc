#!/bin/bash
show=$1

for i in `seq 1 4`; do
    gnuplot <<< "set terminal postscript enhanced color eps
    set output 'r${i}.time.eps'
    set xlabel 'GiB'
    set xtics (0.25,0.5,0.75,1,1.25,1.5,1.75,2)
    set ylabel 'seconds'
    plot 'results${i}' using (\$2/2**30):8 title 'simple O0' with linespoints, '' using (\$2/2**30):10 title 'optimised128npw' with linespoints, '' using (\$2/2**30):12 title 'optimised128pwstrm' with linespoints, '' using (\$2/2**30):14 title 'optimised128pwcmpb' with linespoints, '' using (\$2/2**30):16 title 'optimised256pw' with linespoints
    "
    gnuplot <<< "set terminal postscript enhanced color eps
    set output 'r${i}.speedup.eps'
    set xlabel 'GiB'
    set xtics (0.25,0.5,0.75,1,1.25,1.5,1.75,2)
    set ylabel 'speedup'
    plot 'results${i}' using (\$2/2**30):(\$8/\$10) title 'optimised128npw' with linespoints, '' using (\$2/2**30):(\$8/\$12) title 'optimised128pwstrm' with linespoints, '' using (\$2/2**30):(\$8/\$14) title 'optimised128pwcmpb' with linespoints, '' using (\$2/2**30):(\$8/\$16) title 'optimised256pw' with linespoints
    "
    epstopdf "r${i}.time.eps"
    epstopdf "r${i}.speedup.eps"
    if [[ "$show" == *"t"* ]]; then
        xdg-open "r${i}.time.pdf"
    fi
    if [[ "$show" == *"s"* ]]; then
        xdg-open "r${i}.speedup.pdf"
    fi
done
