set terminal epslatex size 9cm, 4cm
#set terminal pngcairo size 18cm, 10cm

set lmargin at screen 0.1
set rmargin at screen 0.9
set xrange [XMIN:XMAX]
set key autotitle columnheader

unset xtics
set xlabel "t"
set ylabel "compute node id"

plot for [i=0:*] offloads index i using 1:2:3:4:yticlabels(2) with vectors notitle

