#!/bin/bash

ds=ht;

file=$1;

skip=$#;

if [ -f "$file" ];
then
    echo "//Using config file $file";
    . $file;
    skip=0;
else
. ./scripts/tsx/run.config;
fi;

algos=(${ub}/tsx-ht_lazy ${ub}/tsx-ht_harris ${ub}/tsx-ht_java ${ub}/tsx-ht_copy ${ub}/tsx-ht_clht );

params_i=( 512 8192 65536 512 8192 65536 512 8192 65536 512 8192 65536 512 8192 65536 );
params_u=( 80  80   80    60  60   60    40  40   40    20  20   20    10  10   10    );
params_w=( 0   0    0     0   0    0     0   0    0     0   0    0     0   0    0     );
load_factor=1;
np=${#params_i[*]};

cores_backup=$cores;
. ./scripts/config;

nc=$(echo "$cores" | wc -w);
dur_s=$(echo $duration/1000 | bc -l);
na=${#algos[@]};

dur_tot=$(echo "$na*$np*$nc*$repetitions*$dur_s" | bc -l);

printf "#> $na algos, $np params, $nc cores, $repetitions reps of %.2f sec = %.2f sec\n" $dur_s $dur_tot;
printf "#> = %.2f hours\n" $(echo $dur_tot/3600 | bc -l);

cores=$cores_backup;

if [ $do_compile -eq 1 ];
then
    ctarget=tsx${ds};
    for WORKLOAD in 0 2;
    do
	cflags="SET_CPU=$set_cpu WORKLOAD=$WORKLOAD TSX_STATS=2";
	echo "----> Compiling" $ctarget " with flags:" $cflags;
	make $ctarget $cflags >> /dev/null;
	if [ $? -eq 0 ];
	then
	    echo "----> Success!"
	else
	    echo "----> Compilation error!"; exit;
	fi;
	echo "----> Moving binaries to $ub";
	mkdir $ub &> /dev/null;
	bins=$(ls bin/*${ds}*);
	for b in $bins;
	do
	    target=$(echo $ub/${b}"_"$WORKLOAD"_stats" | sed 's/bin\///2g');
	    mv $b $target;
	done
	if [ $? -eq 0 ];
	then
	    echo "----> Success!"
	else
	    echo "----> Cannot mv executables in $ub!"; exit;
	fi;
    done;

    exit 1;
fi;


for ((i=0; i < $np; i++))
do
    initial=${params_i[$i]};
    update=${params_u[$i]};
    range=$((2*$initial));

    workload=${params_w[$i]};
    suffix="_stats"

    if [ "${workload}0" = "0" ];
    then
	workload=0;
    fi;

    algos_w=( "${algos[@]/%/_$workload$suffix}" )
    algos_str="${algos_w[@]}";

    if [ $fixed_file_dat -ne 1 ];
    then
	out="$unm.stats.${ds}.i$initial.u$update.w$workload.dat"
    else
	out="data.stats.${ds}.i$initial.u$update.w$workload.dat"
    fi;

    echo "### params -i$initial -r$range -u$update / keep $keep of reps $repetitions of dur $duration" | tee ${uo}/$out;
    ./scripts/tsx_rep_simple.sh $cores $repetitions $keep "$algos_str" -d$duration -i$initial -r$range -u$update -l$load_factor \
				 | tee -a ${uo}/$out;
done;
