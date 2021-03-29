#!/usr/bin/env perl

use warnings;
use strict;

use feature qw(say);

use Data::Dumper;
use Cwd;
use File::Copy;

eval "use Getopt::Long::Complete 'HelpMessage'";

if($@) {
	use Getopt::Long 'HelpMessage';
}

# https://stackoverflow.com/a/3002214
Getopt::Long::Configure("pass_through");

GetOptions(
    'help|h'          => sub { HelpMessage(0) },
    'nodes|n=n'       => \ ( my $nodes = 16 ),
    'executable|e=s'  => \   my $executable,
    'problem|p=s'     => \   my $problem,
    'name|n=s'        => \   my $name,
    'cpus_per_task=n' => \ ( my $cpus_per_task = 32 ),
    'runlim=s'        => \ ( my $runlim = "/home/biere/bin/runlim" ),
    'time=n'          => \ ( my $time = 3600000 ),
    'space=n'         => \ ( my $space = 80000 ),
) or HelpMessage(1);

if(-e $runlim) {
	$runlim = "$runlim --time-limit=\"$time\" --real-time-limit=\"$time\" --space-limit=\"$space\"";
	say "c Runlim active! Command: $runlim";
} else {
	# Deactivate runlim
	$runlim = "";
	say "c Runlim inactive!";
}

die "Require an executable to run!" unless $executable;
die "Require a problem!" unless $problem;
die "Require a name!" unless $name;

my $paracooba_args = join(" ", @ARGV);
say "c Giving arguments to paracooba: $paracooba_args";

# Create fitting directory in working dir for storing outputs.
my $outdir = cwd() . "/" . $name;
mkdir($outdir) or die "Cold not create output directory $outdir!";

my $binary = $outdir . "/binary";
copy($executable, $binary);
`chmod +x "$binary"`;

# Calling slurm from perl was inspired from here:
# https://hpc.nih.gov/docs/job_dependencies.html

my $common_header = << "END";
echo "c array.sh: name:  \$name"
echo "c array.sh: task:  \$SLURM_ARRAY_TASK_ID"
echo "c array.sh: host:  `hostname`"
echo "c array.sh: start: `date`"

export ASAN_OPTIONS=print_stacktrace=1
export UBSAN_OPTIONS=print_stacktrace=1
END

my $client_job = << "END";
#!/usr/bin/env bash
#SBATCH --error "$outdir/1.err"
#SBATCH --output "$outdir/1.log"
$common_header

echo "c Running job $name!";
$runlim \"$binary\" \"$problem\" \\
	--worker-count $cpus_per_task \\
	--id 1 \\
	$paracooba_args;
END

my $client_job_file = $outdir . "/client.sh";

open(client_job_file_handle, '>', $client_job_file) or die "Could not write client job file $client_job_file!";
print(client_job_file_handle $client_job);
close(client_job_file_handle);

my $client_jobnum = `sbatch --parsable -J $name -n 1 -N 1 -c $cpus_per_task $client_job_file`;
chop $client_jobnum;

my $client_node = "(None)";
while ($client_node eq "(None)") {
	sleep 1;

	my $client_job_squeue = `squeue --job $client_jobnum -h`;
	chop $client_job_squeue;
	my @client_job_fields = split /\s+/, $client_job_squeue;
	$client_node = $client_job_fields[8];
}

say "Client running on node $client_node - Starting daemons with correct remote.";

my $daemons_job = << "END";
#!/usr/bin/env bash
#SBATCH --error "$outdir/%a.err"
#SBATCH --output "$outdir/%a.log"
$common_header

echo "c Running daemon job $name id \$SLURM_ARRAY_TASK_ID!";

$runlim \"$binary\" \\
	--worker-count $cpus_per_task \\
	--id \$SLURM_ARRAY_TASK_ID \\
	--known-remote \"$client_node\" \\
	$paracooba_args;
END

my $daemons_job_file = $outdir . "/daemons.sh";

open(daemons_job_file_handle, '>', $daemons_job_file) or die "Could not write daemons job file $daemons_job_file!";
print(daemons_job_file_handle $daemons_job);
close(daemons_job_file_handle);

`sbatch -J $name -n 1 --array=2-$nodes -c $cpus_per_task $daemons_job_file`;

=head1 NAME

paracooba-slurm-runner - Execute a paracooba job on slurm with correct dependencies!

=head1 SYNOPSIS

  --nodes,-n        Number of nodes
  --executable,-e   Path to parac executable
  --problem,-p      Path to problem to execute
  --name,-n         Name of this run
  --help,-h         Produce this help message

All other arguments are passed to paracooba directly.

=head1 VERSION

0.1

=cut
