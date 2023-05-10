#!/bin/bash
set -e

cd apps
# run memcached and stamp with clobber and pmdk
./run_stamp.sh
cat vacation-pmdk.csv >> ../vacation.csv
cat vacation-clobber.csv >> ../vacation.csv
cat yada-pmdk.csv >> ../yada.csv
cat yada-clobber.csv >> ../yada.csv
./run_memcache.sh mutex pmdk
cat memcached.csv >> ../memcached.csv
./run_memcache.sh mutex clobber
cat memcached.csv >> ../memcached.csv

# run memcached and vacation with mnemosyne
cd ../mnemosyne-gcc/usermode/
./run_vacation.sh
cat vacation.csv >> ../../vacation.csv
./run_memcache.sh
cat memcached.csv >> ../../memcached.csv
cd ../..

