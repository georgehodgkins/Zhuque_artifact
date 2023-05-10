#!/bin/bash
set -e
# This script runs benchmarks and compiles results for the Zhuque and native configurations
mkdir -p /mnt/pmem/wsp

# run the old and new versions of memcached
./run_memcache.sh
cat ./memcached-zhuque.csv >> memcached.csv
cat ./memcached-native.csv >> memcached.csv

./run_new_memcache.sh
cat ./memcached_new-zhuque.csv >> memcached_new.csv
cat ./memcached_new-native.csv >> memcached_new.csv

# run stamp benchmarks
cd stamp/vacation
./run.sh
cat vacation-zhuque.csv >> /apps/vacation.csv
cat vacation-native.csv >> /apps/vacation.csv

cd ../yada
./run.sh
cat yada-zhuque.csv >> /apps/yada.csv
cat yada-native.csv >> /apps/yada.csv
cd ../..

# run sneksit
# not working! aaaah
#printf "Running sneksit (Pyperformance)..."
#cd sneksit
#rm -rf /mnt/pmem/wsp/*
#python3 sneksit.py --psdir /mnt/pmem/wsp > /apps/sneksit.out
#echo "done."

# run libc-bench
printf "Running libc-bench..."
cd ../libc-bench
./libc-bench > /apps/libc-bench.out

