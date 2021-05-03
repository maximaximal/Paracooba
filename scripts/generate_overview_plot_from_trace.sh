#! /usr/bin/env bash

die () {
    echo >&2 "$@"
    exit 1
}

[ "$#" -eq 1 ] ||  die "!! Require trace file as argument!"

function ensure_command_available() {
    if ! command -v $1 &> /dev/null
    then
        echo "!! $1 executable not in PATH!"
        exit
    fi
}

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

ensure_command_available "distrac";
ensure_command_available "gnuplot";

T=$1
N=$(distrac $T --print-node-count)

XMIN=$(distrac $T --print-start-ns)
XMAX=$(distrac $T --print-end-ns)

>&2 echo "Working with $N nodes.";

>&2 echo "Generate Offload Chart Data...";

mkdir /dev/shm/overview

function gen_offloads() {
    for (( i = 0; i < $N; i++ ))
    do
        >&2 echo "   Working on offload data for internal node id $i"
        echo "\"Node $i\""
        distrac --select-nodes $i --omit-csv-header $T -e "offload_task" | awk '{ print $1 " " $3 " 0 " $4 - $3 " " $7; }'
        echo ""
        echo ""
    done > /dev/shm/overview/offloads.data
}

function gen_timings() {
    for (( i = 0; i < $N; i++ ))
    do
        >&2 echo "   Working on finish processing task data for internal node id $i"
        echo "\"Node $i\""
        distrac --select-nodes $i --omit-csv-header $T -e "finish_processing_task" | awk '{ print $1 " n" $3 " " $8; }'
        echo ""
        echo ""
    done > /dev/shm/overview/timings.data
}

function gen_util() {
    for (( i = 0; i < $N; i++ ))
    do
        >&2 echo "   Working on utilization data for internal node id $i"
        echo "\"Node $i\""
        distrac $T -e "worker_idle" "worker_working" --select-nodes $i --omit-csv-header | awk 'function alen(a, i, k) { k = 0; for(i in a) k++; if(k == 0) return 1; return k; }; function aworking(a, i, k) { k = 0; for(i in a) if(a[i]==1) k++; return k; }; $1 == "worker_idle" { print ($2-1) " n" $4 " " (aworking(w) / alen(w)); w[$5]=0; print $2 " n" $4 " " (aworking(w) / alen(w)); }; $1 == "worker_working" { print ($2-1) " n" $4 " " (aworking(w) / alen(w)); w[$5]=1; print $2 " n" $4 " " (aworking(w) / alen(w)) };'
        echo ""
        echo ""
    done > /dev/shm/overview/utilization.data
}

function gen_net_send() {
    for (( i = 0; i < $N; i++ ))
    do
        >&2 echo "   Working on network send kb/s data for internal node id $i"
        echo "\"Node $i\""
        distrac $T --select-nodes $i -e send_msg --omit-csv-header --time-divide 1000000000 | awk 'BEGIN { BYTES_SENT=0; }; "TIME_DIVIDE" == $1 {print $3 " " (BYTES_SENT / 1024.0); BYTES_SENT=0;} "TIME_DIVIDE" != $1 {BYTES_SENT+=$4;}'
        echo ""
        echo ""
    done > /dev/shm/overview/network_send.data
}

# Run all generation at the same time to save runtime.
gen_offloads &
gen_timings &
gen_util &
gen_net_send &

wait

gnuplot -e 'network_send="/dev/shm/overview/network_send.data"' -e 'XMIN="'$XMIN'"' -e 'XMAX="'$XMAX'"' -e 'utilization="/dev/shm/overview/utilization.data"' -e 'offloads="/dev/shm/overview/offloads.data"' -e 'timings="/dev/shm/overview/timings.data"' $DIR/generate_overview_plot_from_trace_plt.gnuplot > ${T}_plots.png
gnuplot -e "set output '${T}_offloads.tex'" -e 'network_send="/dev/shm/overview/network_send.data"' -e 'XMIN="'$XMIN'"' -e 'XMAX="'$XMAX'"' -e 'utilization="/dev/shm/overview/utilization.data"' -e 'offloads="/dev/shm/overview/offloads.data"' -e 'timings="/dev/shm/overview/timings.data"' $DIR/generate_offload_plot.gnuplot

#rm -rf /dev/shm/overview
