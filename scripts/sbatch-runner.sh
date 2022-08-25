#!/usr/bin/env bash
#SBATCH --job-name=para              # Job name
#SBATCH --time=02:00:00              # Time limit hrs:min:sec
#SBATCH --output=parallel-%j.log     # Standard output and error log

PARACOOBA=$1
INPUT=$2
LEADERNODE=$SLURMD_NODENAME
WORKERS=$((SLURM_CPUS_PER_TASK / 2))

if [[ -x "${PARACOOBA}" ]]; then
	echo "c Using paracooba executable $PARACOOBA"
else
	echo "c ERROR: No paracooba executable set in \$PARACOOBA! Value: $PARACOOBA"
	exit 1
fi

echo "c Number of nodes: $SLURM_JOB_NUM_NODES"
echo "c Running on nodes: $SLURM_JOB_NODELIST"
echo "c This script is running on $SLURMD_NODENAME"
echo "c Input: $INPUT"
echo "c Workers: $WORKERS"

ID=$SLURM_JOB_ID
TCP_PORT=$((5000 + $RANDOM % 10000))

echo "c Run paracooba main node..."
echo "c Main node: " $(srun -w"$SLURMD_NODENAME" -n1 -N1 --threads-per-core=2 -c$SLURM_CPUS_PER_TASK /usr/bin/cat /etc/hostname)
srun -w"$SLURMD_NODENAME" -n1 -N1 --threads-per-core=2 -c$SLURM_CPUS_PER_TASK $PARACOOBA --worker $WORKERS -d --id 1 --tcp-listen-address "::" --tcp-listen-port $TCP_PORT $INPUT &
PID=$!

trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT

sleep 2

for (( i = 2 ; i <= $SLURM_JOB_NUM_NODES ; i++ ))
do
	echo "c Schedule worker node $i with $WORKERS workers..."
	srun -n1 -N1 --threads-per-core=2 -c$SLURM_CPUS_PER_TASK --export=PARAC_ID=$i $PARACOOBA --tcp-listen-address "::" --known-remote "$LEADERNODE:$TCP_PORT" --worker $WORKERS -d &
done

wait $PID
