#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

if [ -z ${AWS_BATCH_JOB_MAIN_NODE_PRIVATE_IPV4_ADDRESS+x} ]; then
    aws s3 cp s3://${S3_BKT}/${COMP_S3_PROBLEM_PATH} $DIR/build/problem.cnf
    $DIR/build/parac $DIR/build/problem.cnf $@
else
    $DIR/build/parac --daemon $Q
fi
