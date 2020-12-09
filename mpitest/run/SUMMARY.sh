#!/bin/bash

awk -f SUMMARY.awk $1

#
# cat summaries/*.out | sh ./SUMMARY.sh >summaries/pingpong-20201209.csv 
#
