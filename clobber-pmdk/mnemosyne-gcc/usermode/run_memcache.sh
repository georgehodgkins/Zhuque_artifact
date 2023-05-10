#!/bin/bash
set -e

bin=./build/bench/memcached/memcached-1.2.4-mtm/memcached

if [[ $1 == '-h' ]]
then
	$bin -h
	exit
fi

THREADS="1 2 4 8 16"
WORKLOAD="95 75 25 5"

rm -f memcached.csv
echo -n "Running mnemosyne memcached..."
{
for T in $THREADS; do
	for W in $WORKLOAD; do
		for I in `seq 0 4`; do
			rm -rf /mnt/pmem/*
			cp run_$W.cnf run.cnf
			set +e
			killall -q memcached
			set -e
			sleep 1
			wait
			./build/bench/memcached/memcached-1.2.4-mtm/memcached -u root -p 11215 -l 127.0.0.1 -t $T &
			sleep 1
        	./run_memslap.sh >&data.log
			RATE=`cat data.log | grep 'Net_rate' | awk '{ print $9 }'`
			echo "mnemosyne,${T},${W},${I},${RATE}">> memcached.csv
		done
	done
done
} >&output.log
echo "done."
rm output.log
rm data.log
