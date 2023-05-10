#!/bin/bash
set -e

THREADS="1 2 4 8 16"

TREE=$1
LIB=$2

rm -f vacation.csv
printf "Running vacation-${LIB}-${TREE}..."
for T in $THREADS; do
	for I in `seq 0 4`; do
		rm -rf /mnt/pmem/*
		PMEM_IS_PMEM_FORCE=1 ./vacation-$LIB-$TREE -r100000 -t200000 -n1 -q80 -u99 -c $T >&data.log
		TIME=`cat data.log | grep 'Time' | awk '{ print $3 }'`
		if [ ! -n "$TIME" ]; then
			echo "Failure happened!"
		else
			echo "${LIB}-${TREE},${T},${I},${TIME}">> vacation.csv
		fi
	done
done
echo "done."

rm -f data.log
