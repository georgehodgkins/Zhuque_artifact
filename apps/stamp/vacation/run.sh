#!/bin/bash
set -e

THREADS="1 2 4 8 16"

CONFIGS="zhuque native"
ZHUQ_DIR=/mnt/pmem/wsp

rm -f vacation.csv
printf "Running Zhuque and native vacation..."
for T in $THREADS; do
	for I in `seq 0 4`; do
		for C in $CONFIGS; do
			if [[ $C == 'zhuque' ]]; then
				rm -rf $ZHUQ_DIR/*
				LD_RELOAD=$ZHUQ_DIR ./vacation -r100000 -t200000 -n1 -q80 -u99 -c $T >&data.log
			else
				./vacation -r100000 -t200000 -n1 -q80 -u99 -c $T >&data.log
			fi
			TIME=`cat data.log | grep 'Time' | awk '{ print $3 }'`
			if [ ! -n "$TIME" ]; then
				echo "Failure happened!"
			else
				echo "${C},${T},${I},${TIME}">> vacation-$C.csv
			fi
		done
	done
done
echo "done."

rm -f data.log
