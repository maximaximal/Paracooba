#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

DAEMON_THREADS=$(grep -c ^processor /proc/cpuinfo)
let DAEMON_THREADS=DAEMON_THREADS/2

if [ -z ${AWS_BATCH_JOB_MAIN_NODE_PRIVATE_IPV4_ADDRESS+x} ]; then
    aws s3 cp s3://${S3_BKT}/${COMP_S3_PROBLEM_PATH} $DIR/build/problem.cnf
    $DIR/scripts/generate-cubes.sh $DIR/build/problem.cnf > $DIR/build/problem.icnf
    $DIR/build/parac --resplit-cubes --cadical-cubes-depth 17 --cadical-minimal-cubes-depth 12 $DIR/build/problem.icnf $@
else
    $DIR/build/parac --daemon --resplit-cubes --threads $DAEMON_THREADS $Q
fi

# if [ $? -ne 0 ]; then
#     echo "Return code of parac was not successful! Try to print coredump."
#     coredumpctl dump
# fi
