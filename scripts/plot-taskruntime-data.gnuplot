# Calling this script: gnuplot -e 'file="foofile"' plot-utilization-data.gnuplot --persist

set boxwidth 0.5
set style fill solid
unset xtics
set title ""
set xlabel "Path ID"
set ylabel "Solve time [s]"
plot file using 0:1:xticlabels(2) with boxes notitle
