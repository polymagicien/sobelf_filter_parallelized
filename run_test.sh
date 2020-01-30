#!/bin/bash

make

let "NODE = 1"
let "PROCESSES = 2"
INPUT_DIR=images/original
OUTPUT_DIR=images/processed
PERFORMANCE_FILE="performance/perf_NODE_{$NODE}_proc_{$PROCESSES}.txt"
mkdir $OUTPUT_DIR 2>/dev/null

touch $PERFORMANCE_FILE
rm $PERFORMANCE_FILE

echo $PERFORMANCE_FILE

for i in $INPUT_DIR/*gif ; do
    DEST=$OUTPUT_DIR/`basename $i .gif`-sobel.gif
    echo "Running test on $i -> $DEST"
    salloc -N $NODE -n $PROCESSES mpirun ./sobelf $i $DEST $PERFORMANCE_FILE
done
