#! /usr/bin/env bash

FORMULA=$1
N=$2

./paracs --worker 3 --distrac-enable --distrac-output ${N}_1.trace --id 1 $3 --tcp-listen-port=18100 $FORMULA > ${N}_1.log 2>&1 &
sleep 1
./paracs --worker 3 --distrac-enable --distrac-output ${N}_2.trace --id 2 $3 --known-remote localhost:18100 --auto-shutdown-after-finished-client > ${N}_2.log 2>&1 &
./paracs --worker 3 --distrac-enable --distrac-output ${N}_3.trace --id 3 $3 --known-remote localhost:18100 --auto-shutdown-after-finished-client > ${N}_3.log 2>&1 &
./paracs --worker 3 --distrac-enable --distrac-output ${N}_4.trace --id 4 $3 --known-remote localhost:18100 --auto-shutdown-after-finished-client > ${N}_4.log 2>&1

cat "${N}_1.trace" "${N}_2.trace" "${N}_3.trace" "${N}_4.trace" > "${N}.trace";

rm ${N}_*.trace
