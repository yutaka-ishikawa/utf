#!/bin/bash

PROF_DIR=./profdata/
REP_DIR=./rep

cd $PROF_DIR
for i in  `seq 1 15`
do
    fapppx -A -d ${REP_DIR}$i -Icpupa,nompi -tcsv -o pa$i.csv
done
