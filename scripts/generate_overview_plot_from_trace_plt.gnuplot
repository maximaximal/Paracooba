set multiplot layout 3,1

set palette defined ( 0 "red", 1 "yellow", 2 "cyan", 3 "blue", 4 "magenta")

set lmargin at screen 0.1
set rmargin at screen 0.9
unset xtics

set title "Task Offloads"
plot offloads using 1:2:3:4 with vectors notitle
set title "Utilization in the Moment of Offload"
plot offloads using 1:5:2 with impulses lc palette notitle
set title "Task Runtime"
plot timings using 1:3:2 with impulses lc palette notitle

unset multiplot
