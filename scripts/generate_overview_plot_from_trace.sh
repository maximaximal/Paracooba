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

echo "Working with $N nodes.";

echo "Generate Offload Chart Data...";

mkdir /dev/shm/overview

distrac --omit-csv-header $T -e "offload_task" | awk '{ print $1 " " $3 " 0 " $4 - $3 " " $7; }' > /dev/shm/overview/offloads.data
distrac --omit-csv-header $T -e "finish_processing_task" | awk '{ print $1 " " $3 " " $8; }' > /dev/shm/overview/timings.data

# for (( i = 0; i < $N; i++ ))
# do
# done

gnuplot -e 'offloads="/dev/shm/overview/offloads.data"' -e 'timings="/dev/shm/overview/timings.data"' $DIR/generate_overview_plot_from_trace_plt.gnuplot --persist

#rm -rf /dev/shm/overview
