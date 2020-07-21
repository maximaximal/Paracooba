# Calling this script: gnuplot -e 'file="foofile"' plot-utilization-data.gnuplot --persist

plot for [col=2:*] file using 0:col with lines title columnheader
