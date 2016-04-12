#!/bin/bash

if [ $# -eq 0 ];
then
    echo "Usage: $0 \"cores\" num_repetitions value_to_keep \"executable1 excutable2 ...\" [params]";
    echo "  where \"cores\" can be the actual thread num to use, such as \"1 10 20\", or"
    echo "  one of the predefined specifications for that platform (e.g., socket -- see "
    echo "  scripts/config)";
    echo "  and value_to_keep can be the min, max, or median";
    exit;
fi;

cores=$1;
shift;

reps=$1;
shift;

source scripts/lock_exec;
source scripts/config;
source scripts/help;

result_type=$1;

if [ "$result_type" = "max" ];
then
    run_script="./scripts/run_rep_tsx_max.sh $reps";
    echo "# Result from $reps repetitions: max";
    shift;

elif [ "$result_type" = "min" ];
then
    run_script="./scripts/run_rep_tsx_min.sh $reps";
    echo "# Result from $reps repetitions: min";
    shift;
elif [ "$result_type" = "median" ];
then
    run_script="./scripts/run_rep_tsx_med.sh $reps";
    echo "# Result from $reps repetitions: median";
    shift;
else
    run_script="./scripts/run_rep_tsx_max.sh $reps";
    echo "# Result from $reps repetitions: max (default). Available: min, max, median";
fi;

progs="$1";
shift;
progs_num=$(echo $progs | wc -w);
params="$@";

progs_stripped=$(echo $progs | sed -e 's/bin//g' -e 's/[\.\/]//g');

print_n   "#       " "%-95s " "$progs_stripped" "\n"

print_rep "#cores  " $progs_num "commit_rate commits     1st_trial   2nd_trial   3rd_trial   1st_abort   2nd_abort   3rd_abort   " "\n"

stats="trials_round1 trials_round2 trials_round3 aborts_round1 aborts_round2 aborts_round3 commits";

printf "%-8d" 1;
thr1="";
for p in $progs;
do
    printf "%-11s " $($run_script ./$p $params -n1);
done;

echo "";

for c in $cores
do
    if [ $c -eq 1 ]
    then
	continue;
    fi;

    printf "%-8d" $c;

    i=0;
    for p in $progs;
    do
        printf "%-11s " $($run_script ./$p $params -n$c);
    done;
    echo "";
done;

source scripts/unlock_exec;
