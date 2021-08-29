#!/usr/bin/env bash

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

mkdir /dev/shm/offloads

>&2 echo "Working with $N nodes.";

function gen_offloads() {
    for (( i = 0; i < $N; i++ ))
    do
        >&2 echo "   Working on offload data for internal node id $i"
        echo "\"Node $i\""
        distrac --select-nodes $i --omit-csv-header $T -e "offload_task" | awk '{ print $1 " " $3 " 0 " $4 - $3 " " $7; }'
        echo ""
        echo ""
    done > /dev/shm/offloads/offloads.data
}

gen_offloads

gnuplot -e 'XMIN="'$XMIN'"' -e 'XMAX="'$XMAX'"' -e 'offloads="/dev/shm/offloads/offloads.data"' \
    -e 'set terminal epslatex size 20, 10cm; set output "'${T}_offloads.tex'"' \
    $DIR/generate_offload_plot.gnuplot

gnuplot -e 'XMIN="'$XMIN'"' -e 'XMAX="'$XMAX'"' -e 'offloads="/dev/shm/offloads/offloads.data"' \
    -e 'set terminal pngcairo size 20cm, 10cm' \
    $DIR/generate_offload_plot.gnuplot > ${T}_offloads.png &

wait

echo "Output: ${T}_offloads.tex"
echo "Output: ${T}_offloads.png"

#wait

rm -rf /dev/shm/offloads
