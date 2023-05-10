#!/bin/bash
set -e

cd stamp/vacation
./run.sh rbtree pmdk
cp vacation.csv ../../vacation-pmdk.csv
./run.sh rbtree clobber
cp vacation.csv ../../vacation-clobber.csv

cd ../yada
./run.sh pmdk
cp yada.csv ../../yada-pmdk.csv
./run.sh clobber
cp yada.csv ../../yada-clobber.csv
cd ../../


