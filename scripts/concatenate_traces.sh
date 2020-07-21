#! /usr/bin/env bash

if [[ $# -lt 2 ]]; then
    echo "Requires arguments: <IN1> <IN2> <IN N> ... <OUT>"
    exit
fi

OUT=${@: -1}

{
    while (( $# > 1 ))
    do
        if compgen -G "$1/*.bin" > /dev/null; then
            cat $1/*.bin
            shift
        else
            shift
        fi
    done
} > $OUT
