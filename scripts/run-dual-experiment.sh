#! /usr/bin/env bash

FORMULA=$1
N=$2

./parac --worker 1 --distrac-enable --distrac-output ${N}_1.trace --id 1 $3 --tcp-listen-port=18100 $FORMULA > ${N}_1.log 2>&1 &
./parac --worker 8 --distrac-enable --distrac-output ${N}_2.trace --id 2 $3 --known-remote localhost:18100 --auto-shutdown-time 3000 > ${N}_2.log 2>&1

cat "${N}_1.trace" "${N}_2.trace" > "${N}.trace";

rm "${N}_1.trace"
rm "${N}_2.trace"
