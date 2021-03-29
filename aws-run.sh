#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

#DAEMON_THREADS=$(grep -c ^processor /proc/cpuinfo)
#let DAEMON_THREADS=DAEMON_THREADS/2

if [ "$AWS_BATCH_JOB_MAIN_NODE_INDEX" = "$AWS_BATCH_JOB_NODE_INDEX" ]; then
    # Main node detected!
    ip=$(/sbin/ip -o -4 addr list eth0 | awk '{print $4}' | cut -d/ -f1)
    echo "main IP: ${ip}"
    aws s3 cp s3://${S3_BKT}/${COMP_S3_PROBLEM_PATH} $DIR/build/problem.cnf
    $DIR/build/paracs --cadical-cubes --initial-cube-depth 15 --initial-minimal-cube-depth 12 --resplit $DIR/build/problem.cnf --concurrent-cube-tree-count 4 --worker $NUM_PROCESSES -d --tcp-listen-address 0.0.0.0 -t $@
else
    echo "c DAEMON NODE: Trying to connect to IP ${AWS_BATCH_JOB_MAIN_NODE_PRIVATE_IPV4_ADDRESS}"
    ping -c 5 ${AWS_BATCH_JOB_MAIN_NODE_PRIVATE_IPV4_ADDRESS}
    $DIR/build/paracs --worker $NUM_PROCESSES --known-remote ${AWS_BATCH_JOB_MAIN_NODE_PRIVATE_IPV4_ADDRESS} --auto-shutdown-after-finished-client --tcp-listen-address 0.0.0.0 -d -t $Q
fi

# if [ $? -ne 0 ]; then
#    echo "Return code of parac was not successful! Try to print coredump."
#     coredumpctl dump
# fi
