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

e>&2 cho "Working with $N nodes.";

e>&2 cho "Generate Offload Chart Data...";

mkdir /dev/shm/overview

for (( i = 0; i < $N; i++ ))
do
    echo "\"Node $i\""
    distrac --select-nodes $i --omit-csv-header $T -e "offload_task" | awk '{ print $1 " " $3 " 0 " $4 - $3 " " $7; }'
    echo ""
    echo ""
done > /dev/shm/overview/offloads.data

for (( i = 0; i < $N; i++ ))
do
    echo "\"Node $i\""
    distrac --select-nodes $i --omit-csv-header $T -e "finish_processing_task" | awk '{ print $1 " n" $3 " " $8; }'
    echo ""
    echo ""
done > /dev/shm/overview/timings.data

for (( i = 0; i < $N; i++ ))
do
    >&2 echo "   Working on utilization data for internal node id $i"
    echo "\"Node $i\""
    distrac $T -e "worker_idle" "worker_working" --select-nodes $i --omit-csv-header | awk 'function alen(a, i, k) { k = 0; for(i in a) k++; if(k == 0) return 1; return k; }; function aworking(a, i, k) { k = 0; for(i in a) if(a[i]==1) k++; return k; }; $1 == "worker_idle" { print ($2-1) " n" $4 " " (aworking(w) / alen(w)); w[$5]=0; print $2 " n" $4 " " (aworking(w) / alen(w)); }; $1 == "worker_working" { print ($2-1) " n" $4 " " (aworking(w) / alen(w)); w[$5]=1; print $2 " n" $4 " " (aworking(w) / alen(w)) };'
    echo ""
    echo ""
done > /dev/shm/overview/utilization.data

gnuplot -e 'utilization="/dev/shm/overview/utilization.data"' -e 'offloads="/dev/shm/overview/offloads.data"' -e 'timings="/dev/shm/overview/timings.data"' $DIR/generate_overview_plot_from_trace_plt.gnuplot > ${T}_plots.png

rm -rf /dev/shm/overview
