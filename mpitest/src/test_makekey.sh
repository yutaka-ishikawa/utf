#!/bin/bash

echo "GCC"
./test_makekey data_makekey_gcc

echo "CROSS"
./test_makekey data_makekey_cross
