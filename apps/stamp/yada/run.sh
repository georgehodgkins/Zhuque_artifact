#!/bin/bash
set -e

ANGLE="15 20 25 30"
CONFIGS="native zhuque"
ZHUQ_DIR=/mnt/pmem/wsp

rm -f yada.csv
printf "Running Zhuque & native yada..."
for A in $ANGLE; do
	for C in $CONFIGS; do
		if [[ $C == 'zhuque' ]]; then
			rm -rf $ZHUQ_DIR/*
			LD_RELOAD=$ZHUQ_DIR ./yada -a $A -i inputs/ttimeu10000.2 >&data.log
		else
			./yada -a $A -i inputs/ttimeu10000.2 >&data.log
		fi
		TIME=`cat data.log | grep 'Elapsed time' | awk '{ print $4 }'`
		echo "${C},${A},${I},${TIME}">> yada-$C.csv
	done
done
echo "done."

