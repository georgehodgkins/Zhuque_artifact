#!/bin/bash
set -e

THREADS="1 2 4 8 16"
WORKLOAD="95 75 25 5"

LOCK=$1
LIB=$2

unset LD_PRELOAD

printf "Running %s-%s memcached..." $LIB $LOCK
{
for T in $THREADS; do
	for W in $WORKLOAD; do
		for I in `seq 0 4`; do
			rm -rf /mnt/pmem/*
			cp ../mnemosyne-gcc/usermode/run_$W.cnf ../mnemosyne-gcc/usermode/run.cnf
			set +e
			killall memcached
			set -e
			MEMCACHE_DIR="memcached-${LOCK}-${LIB}"
			if [[ $LOCK == 'mutex' ]]; then
				export LIBTXLOCK=tas
				LD_PRELOAD=$(realpath ../taslock/tl-pthread-mutex.so) PMEM_IS_PMEM_FORCE=1 \
					./$MEMCACHE_DIR/memcached -u root -p 11215 -l 127.0.0.1 -t $T &
			else
				PMEM_IS_PMEM_FORCE=1 ./$MEMCACHE_DIR/memcached -u root -p 11215 -l 127.0.0.1 -t $T &
			fi
			sleep 1
			cd ../mnemosyne-gcc/usermode
	        	./run_memslap.sh >&../../apps/data.log
			cd ../../apps
			RATE=`cat data.log | grep 'Net_rate' | awk '{ print $9 }'`
            if [[ $LOCK == 'mutex' ]]; then
	            echo "${LIB}-spinlock,${T},${W},${I},${RATE}">> memcached.csv
	else
                echo "${LIB}-${LOCK},${T},${W},${I},${RATE}">> memcached.csv
            fi
		done
	done
done
} >&output.log
echo "done."
rm output.log
rm data.log

