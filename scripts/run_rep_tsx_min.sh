#!/bin/bash

source scripts/config;

reps=$1;
shift;
#stat=$1;
#shift
prog="$1";
shift;
params="$@";

un=$(uname -n);
tmp=data/run_rep_min.${un}.tmp
printf "" > $tmp;

for r in $(seq 1 1 $reps);
do
    $run_script ./$prog $params | grep "tsx stats" | cut -d':' -f2 >> $tmp;
done;

HEAD=head;
if [ "$(uname -n)" = ol-collab1 ];
then
    HEAD=/usr/gnu/bin/head
fi;

sort -n $tmp | ${HEAD} -n1;

