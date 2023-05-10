#!/bin/bash
set -e

THREADS="1 2 4 8 16"
WORKLOAD="95 75 25 5"
CONFIGS="native zhuque"
ZHUQ_DIR=/mnt/pmem/wsp

# this directory contains the configuration parameters and memslap runner script
MNEM_DIR=/clobber-pmdk/mnemosyne-gcc/usermode

unset LD_PRELOAD

printf "Running Zhuque & native memcached 1.2.5..."
{
for T in $THREADS; do
	for W in $WORKLOAD; do
		for C in $CONFIGS; do
			for I in `seq 0 4`; do
				cp $MNEM_DIR/run_$W.cnf $MNEM_DIR/run.cnf
				set +e
				killall memcached
				set -e
				if [[ $C == 'zhuque' ]]; then
					rm -rf $ZHUQ_DIR/*
					LD_RELOAD=$ZHUQ_DIR ./memcached/memcached -u root -p 11215 -l 127.0.0.1 -t $T &
				else # native
					./memcached/memcached -u root -p 11215 -l 127.0.0.1 -t $T &
				fi
				sleep 1
				cd $MNEM_DIR
				./run_memslap.sh >& /apps/data.log
				cd /apps
				RATE=`cat data.log | grep 'Net_rate' | awk '{ print $9 }'`
				echo "${C},${T},${W},${I},${RATE}">> memcached-$C.csv
			done
		done
	done
done
} >&output.log
echo "done."
rm output.log
rm data.log

