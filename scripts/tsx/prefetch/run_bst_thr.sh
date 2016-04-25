#!/bin/bash

ds=bst;

file=$1;

skip=$#;

if [ -f "$file" ];
then
    echo "//Using config file $file";
    . $file;
    skip=0;
else
. ./scripts/tsx/prefetch/run.config;
fi;

algos=( ${ub}/tsx-bst_tk ${ub}/tsx-bst_aravind_cas ${ub}/tsx-bst_aravind_tsx );

params_i=( 1024 16384 65536 1024 16384 65536 1024 16384 65536 1024 16384 65536 1024 16384 65536 1024 16384 65536 1024 16384 65536 1024 16384 65536 1024 16384 65536 );
params_u=( 80   80    80    40   40    40    10   10    10    80   80    80    40   40    40    10   10    10    80   80    80    40   40    40    10   10    10    );
params_pf=(0    0     0     0    0     0     0    0     0     2    2     2     2    2     2     2    2     2     1    1     1     1    1     1     1    1     1     );

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
    for PREFETCH in 0 1 2;
    do
	cflags="SET_CPU=$set_cpu TSX_PREFETCH=$PREFETCH";
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
	    target=$(echo $ub/${b}"_pf"$PREFETCH | sed 's/bin\///2g');
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

    prefetch=${params_pf[$i]};
    if [ "${prefetch}0" = "0" ];
    then
	prefetch=0;
    fi;

    algos_w=( "${algos[@]/%/_pf$prefetch$suffix}" )
    algos_str="${algos_w[@]}";

    if [ $fixed_file_dat -ne 1 ];
    then
	out="$unm.thr.${ds}.i$initial.u$update.pf$prefetch.dat"
    else
	out="data.thr.${ds}.i$initial.u$update.pf$prefetch.dat"
    fi;

    echo "### params -i$initial -r$range -u$update / keep $keep of reps $repetitions of dur $duration" | tee ${uo}/$out;
    ./scripts/scalability_rep_simple.sh $cores $repetitions $keep "$algos_str" -d$duration -i$initial -r$range -u$update \
				 | tee -a ${uo}/$out;
done;
