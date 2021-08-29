set lmargin at screen 0.1
set rmargin at screen 0.9
set xrange [XMIN:XMAX]
set yrange [0:1]
set key autotitle columnheader

unset xtics
set xlabel "t"
set ylabel "utilization (0..1)"
set title "Runner Workload"

plot for [i=0:*] utilization index i using 1:3 with lines lw 2 title columnheader(1)
