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
tmp=data/run_rep_max.${un}.tmp
tmp2=data/run_rep_max.2.${un}.tmp
printf "" > $tmp;
printf "" > $tmp2;

for r in $(seq 1 1 $reps);
do
    $run_script ./$prog $params >> $tmp2;
    grep "tsx stats" -w $tmp2 | cut -d':' -f2 | tr -d '\n' >> $tmp;
    grep "Trial 0" -w $tmp2 | cut -d':' -f2 | tr -d '\n|' >> $tmp;
    grep "Trial 1" -w $tmp2 | cut -d':' -f2 | tr -d '\n|' >> $tmp;
    grep "Trial 2" -w $tmp2 | cut -d':' -f2 | tr -d '|' >> $tmp;
    rm $tmp2;
done;

TAIL=tail;
if [ "$(uname -n)" = ol-collab1 ];
then
    TAIL=/usr/gnu/bin/tail
fi;

sort -n $tmp | ${TAIL} -n1;

