#!/bin/bash

# TO RUN MAINS ON SET OF DATA
# ./run_test sobelf main_filename
if [ "$1" = "sobelf" ]
then
    make

    let "NODE = 1"
    let "PROCESSES = 8"
    INPUT_DIR=images/original
    OUTPUT_DIR=images/processed
    PERFORMANCE_FILE="performance/${2}_N_${NODE}_n_${PROCESSES}.txt"
    mkdir $OUTPUT_DIR 2>/dev/null

    touch $PERFORMANCE_FILE
    rm $PERFORMANCE_FILE

    echo $PERFORMANCE_FILE

    for i in $INPUT_DIR/*gif ; do
        DEST=$OUTPUT_DIR/`basename $i .gif`-sobel.gif
        echo "Running test on $i -> $DEST"
        salloc -N $NODE -n $PROCESSES mpirun ./$2 $i $DEST $PERFORMANCE_FILE
    done
fi
if [ "$1" = "test_pixels" ]
then
    make tester
    let "NODE = 1"
    let "PROCESSES = 1"
    INPUT_DIR=images/original
    DEST=test_results/ratio_pixels.txt

    touch $DEST
    rm $DEST

    for i in $INPUT_DIR/*gif ; do
        echo "Running test on $i -> $DEST"
        salloc -N $NODE -n $PROCESSES mpirun ./test 2 $i $DEST
    done
fi
