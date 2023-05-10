#!/bin/bash
set -e

ANGLE="15 20 25 30"
LIB=$1

rm -f yada.csv
printf "Running yada-${LIB}..."
for A in $ANGLE; do
	rm -rf /mnt/pmem/*
	PMEM_IS_PMEM_FORCE=1 ./yada-$LIB -a $A -i inputs/ttimeu10000.2 >&data.log
	TIME=`cat data.log | grep 'Elapsed time' | awk '{ print $4 }'`
	echo "${LIB},${A},${I},${TIME}">> yada.csv
done
echo "done."

