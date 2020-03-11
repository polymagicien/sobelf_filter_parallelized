#!/bin/bash

# TO RUN MAINS ON SET OF DATA
# ./run_test sobelf main_filename
if [ "$1" = "on_img" ]
then
    # 1 : test to run
    # 2 : binary tester
    # ./run_test #1 #2
    
    let "NODE = 1"
    let "PROCESSES = 6"
    INPUT_DIR=images/original
    OUTPUT_DIR=images/processed
    #PERFORMANCE_FILE="performance/${2}_N_${NODE}_n_${PROCESSES}.txt"
    PERFORMANCE_FILE="perf_init_jonas.txt"
    mkdir $OUTPUT_DIR 2>/dev/null

    touch $PERFORMANCE_FILE
    rm $PERFORMANCE_FILE

    echo $PERFORMANCE_FILE

    for i in $INPUT_DIR/*gif ; do
        dest_filename=$OUTPUT_DIR/`basename $i .gif`-sobel.gif
        echo "Running test on $i -> $dest_filename"
        salloc -N $NODE -n $PROCESSES mpirun ./$2 $i $dest_filename $PERFORMANCE_FILE
    done
fi

if [ "$1" = "on_img_threads" ]
then
    # 1 : test to run
    # 2 : binary tester
    # ./run_test #1 #2 
    
    let "FROMT=1"
    let "TOT=8"
    let "FROMP=1"
    let "TOP=10"
    let "NODES=1"
    let "PROC_BY_NODE=1"
    let "BETA=0"
    INPUT_DIR=images/original
    OUTPUT_DIR=images/processed
    #PERFORMANCE_FILE="performance/${2}_N_${NODE}_n_${PROCESSES}.txt"
    PERFORMANCE_FILE="perf_init_jonas.txt"
    mkdir $OUTPUT_DIR 2>/dev/null

    touch $PERFORMANCE_FILE
    rm $PERFORMANCE_FILE

    echo $PERFORMANCE_FILE
    for BETA in $(seq 0 0)
    do
        for PROCESSES in $(seq $FROMP $TOP)
        do
            for THREADS in $(seq $FROMT $TOT)
            do
                ((PROC_BY_NODE= 8/$THREADS))
                ((NODES= $PROCESSES%$PROC_BY_NODE))
                if [ $NODES = "0" ]
                then 
                    ((NODES= $PROCESSES/$PROC_BY_NODE))
                else
                    ((NODES= $PROCESSES/$PROC_BY_NODE+1))
                fi
                for i in $INPUT_DIR/*gif ; do
                    dest_filename=$OUTPUT_DIR/`basename $i .gif`-sobel.gif
                    echo "Running test on $i -> $dest_filename with $THREADS threads, $PROCESSES processes on $NODES nodes"
                    for j in $(seq 1 10)
                    do
                        OMP_NUM_THREADS=$THREADS salloc -N $NODES -c $THREADS -n $PROCESSES mpirun ./$2 $i $dest_filename $3 $BETA
                    done
                done
            done
        done
    done
fi

if [ "$1" = "on_processes" ]
then
    # 1 : test to run
    # 2 : binary tester
    # 3 : input_file
    # ./run_test #1 #2 #3 

    let "FROM=8"
    let "TO=8"
    let "NODE = 1"

    INPUT_DIR=images/original
    OUTPUT_DIR=images/processed
    source_filename=`basename $3 .gif`
    PERFORMANCE_FILE="performance/on_processes/${2}_${source_filename}_N_${NODE}.txt"
    mkdir $OUTPUT_DIR 2>/dev/null
    touch $PERFORMANCE_FILE
    rm $PERFORMANCE_FILE

    for PROCESSES in $(seq $FROM $TO)
    do
        dest_filename=$OUTPUT_DIR/`basename $i .gif`-sobel.gif
        echo "Running test on $3 -> $dest_filename"
        salloc -N $NODE -n $PROCESSES mpirun ./$2 $3 $dest_filename $PERFORMANCE_FILE
    done
fi

if [ "$1" = "initial" ]
then
    # 1 : test to run
    # 2 : binary tester
    # 3 : input_file
    # ./run_test #1 #2 #3 S

    INPUT_DIR=images/original
    OUTPUT_DIR=images/processed
    source_filename=`basename $3 .gif`
    PERFORMANCE_FILE="performance/on_processes/${2}_${source_filename}_N_${NODE}S.txt"
    mkdir $OUTPUT_DIR 2>/dev/null
    touch $PERFORMANCE_FILE
    rm $PERFORMANCE_FILE
    for j in $(seq 1 20); do
        for i in $INPUT_DIR/*gif ; do
            dest_filename=$OUTPUT_DIR/`basename $i .gif`-sobel.gif
            echo "Running test on $i -> $dest_filename with $THREADS threads, $PROCESSES processes on $NODES nodes"
            salloc -N 1 -n 1 mpirun ./$2 $i $dest_filename $PERFORMANCE_FILE
        done
    done
fi



