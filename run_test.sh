#!/bin/bash

# TO RUN MAINS ON SET OF DATA

if [ "$1" = "global" ]
then
    if [ "$2" = "-h" ]
    then
        echo "USAGE: ./run_test.sh global [executable] [output_file] [until_process] [until_threads] [repetition] [gpu_activated] [beta]"
    else
        # 1 : test to run
        # 2 : binary tester
        # 3 : output_file
        # 4 : until this number of process
        # 5 : until this number of threads
        # 6 : number of repetition
        # 7 : gpu activated
        # 8 : beta activated
        # ./run_test #1 #2 
        
        let "NODES=1"
        let "PROC_BY_NODE=1"
        INPUT_DIR=images/original
        OUTPUT_DIR=images/processed
        mkdir $OUTPUT_DIR 2>/dev/null

        touch $PERFORMANCE_FILE
        rm $PERFORMANCE_FILE

        echo $PERFORMANCE_FILE
        for PROCESSES in $(seq 1 $4); do
            for THREADS in $(seq 1 $5); do
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
                    for j in $(seq 1 $6); do #to make a mean
                        OMP_NUM_THREADS=$THREADS salloc -N $NODES -c $THREADS -n $PROCESSES mpirun ./$2 $i $dest_filename -file $3 -beta $8 -rootwork 1 -verifgif 0 -gpu $7
                    done
                done
            done
        done
    fi
fi


if [ "$1" = "initial" ]
then
    # 1 : test to run
    # 2 : binary tester
    # ./run_test #1 #2

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
            OMP_NUM_THREADS=$t salloc -N 1 -n 1 -c $t mpirun ./$2 $i $dest_filename $PERFORMANCE_FILE
        done
    done
fi


if [ "$1" = "threads_method_2" ] #need to be cleaned
then
    # 1 : test to run
    # 2 : binary tester
    # 3 : output_file
    # ./run_test #1 #2 #3

    INPUT_DIR=images/original
    OUTPUT_DIR=images/processed
    source_filename=`basename $3 .gif`
    mkdir $OUTPUT_DIR 2>/dev/null
    for t in $(seq 1 6); do
        for i in $INPUT_DIR/*gif ; do
            dest_filename=$OUTPUT_DIR/`basename $i .gif`-sobel.gif
            echo "Running test on $i -> $dest_filename with $t threads, 1 processes on 1 nodes"
            OMP_NUM_THREADS=$t salloc -N 1 -n 1 -c $t mpirun ./$2 $i $dest_filename -file $3
        done
    done
fi


if [ "$1" = "no_working_root" ]
then
    # 1 : test to run
    # 2 : binary tester
    # 3 : output_file
    # ./run_test #1 #2 #3 

    let "NODES = 1"
    let "res = 0"

    INPUT_DIR=images/original
    OUTPUT_DIR=images/processed
    source_filename=`basename $3 .gif`
    mkdir $OUTPUT_DIR 2>/dev/null
    for w in $(seq 0 1); do
        for p in $(seq 2 8); do
            for i in $INPUT_DIR/*gif ; do
                dest_filename=$OUTPUT_DIR/`basename $i .gif`-sobel.gif
                echo "Running test on $i -> $dest_filename with $p processes on $NODES nodes"
                OMP_NUM_THREADS=1 salloc -N 1 -n $p -c 1 mpirun ./$2 $i $dest_filename -file $3 -beta 1 -rootwork $w -verifgif 1
            done
        done
    done
fi




