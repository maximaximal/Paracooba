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

mkdir /dev/shm/utilization

>&2 echo "Working with $N nodes.";

function gen_util() {
    for (( i = 0; i < $N; i++ ))
    do
        >&2 echo "   Working on utilization data for internal node id $i"
        echo "\"Node $i\""
        distrac $T -e "worker_idle" "worker_working" --select-nodes $i --omit-csv-header | awk 'function alen(a, i, k) { k = 0; for(i in a) k++; if(k == 0) return 1; return k; }; function aworking(a, i, k) { k = 0; for(i in a) if(a[i]==1) k++; return k; }; $1 == "worker_idle" { print ($2-1) " n" $4 " " (aworking(w) / alen(w)); w[$5]=0; print $2 " n" $4 " " (aworking(w) / alen(w)); }; $1 == "worker_working" { print ($2-1) " n" $4 " " (aworking(w) / alen(w)); w[$5]=1; print $2 " n" $4 " " (aworking(w) / alen(w)) };'
        echo ""
        echo ""
    done > /dev/shm/utilization/utilization.data
}

gen_util

OUTT="${T//./_}"

gnuplot -e 'XMIN="'$XMIN'"' -e 'XMAX="'$XMAX'"' -e 'utilization="/dev/shm/utilization/utilization.data"' \
    -e 'set terminal epslatex size 20cm, 10cm; set output "'${OUTT}_utilization.tex'"' \
    $DIR/generate_utilization_plot.gnuplot &
gnuplot -e 'XMIN="'$XMIN'"' -e 'XMAX="'$XMAX'"' -e 'utilization="/dev/shm/utilization/utilization.data"' \
    -e 'set terminal pngcairo size 20cm, 10cm' \
    $DIR/generate_utilization_plot.gnuplot > ${T}_utilization.png  &

wait

echo "Output: ${T}_utilization.tex"
echo "Output: ${T}_utilization.png"

rm -rf /dev/shm/utilization
