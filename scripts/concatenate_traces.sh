#! /usr/bin/env bash

if [[ $# -lt 2 ]]; then
    echo "Requires arguments: <IN1> <IN2> <IN N> ... <OUT>"
    exit
fi

OUT=${@: -1}

{
    while (( $# > 1 ))
    do
        cat $1/*.bin
        shift
    done
} > $OUT
