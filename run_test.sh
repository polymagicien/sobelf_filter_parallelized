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


if [ "$1" = "test_pixels" ]
then
    make tester
    let "NODE = 1"
    let "PROCESSES = 1"
    INPUT_DIR=images/original
    dest_filename=test_results/ratio_pixels.txt

    touch $dest_filename
    rm $dest_filename

    for i in $INPUT_DIR/*gif ; do
        echo "Running test on $i -> $dest_filename"
        salloc -N $NODE -n $PROCESSES mpirun ./test 2 $i $dest_filename
    done
fi
