#!/bin/bash
set -e
# This script runs benchmarks and compiles results for the Zhuque and native configurations
mkdir -p /mnt/pmem/wsp

# run the old and new versions of memcached
#./run_memcache.sh
rm -rf /mnt/pmem/wsp/*
/run_memcache.py zhuque_old
cat ./memcached-zhuque_old.csv >> memcached.csv
/run_memcache.py native_old
cat ./memcached-native_old.csv >> memcached.csv

#./run_new_memcache.sh
rm -rf /mnt/pmem/wsp/*
/run_memcache.py zhuque_new
cat ./memcached-zhuque_new.csv >> memcached_new.csv
/run_memcache.py native_new
cat ./memcached-native_new.csv >> memcached_new.csv

# run stamp benchmarks
rm -rf /mnt/pmem/wsp/*
cd stamp/vacation
./run.sh
cat vacation-zhuque.csv >> /apps/vacation.csv
cat vacation-native.csv >> /apps/vacation.csv

rm -rf /mnt/pmem/wsp/*
cd ../yada
./run.sh
cat yada-zhuque.csv >> /apps/yada.csv
cat yada-native.csv >> /apps/yada.csv
cd ../..

# run sneksit
printf "Running sneksit (Pyperformance)..."
cd sneksit
rm -rf /mnt/pmem/wsp/*
python3 sneksit.py --psdir /mnt/pmem/wsp > /apps/sneksit.out
echo "done."

# run libc-bench
printf "Running libc-bench..."
rm -rf /mnt/pmem/wsp/*
cd ../libc-bench
./libc-bench > /apps/libc-bench-native.out
LD_RELOAD=/mnt/pmem/wsp ./libc-bench > /apps/libc-bench-zhuque.out
echo "done."
