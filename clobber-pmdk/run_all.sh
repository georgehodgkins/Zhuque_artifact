#!/bin/bash
set -e

cd apps
# run memcached and stamp with clobber and pmdk
./run_stamp.sh
cat vacation-pmdk.csv >> ../vacation.csv
cat vacation-clobber.csv >> ../vacation.csv
cat yada-pmdk.csv >> ../yada.csv
cat yada-clobber.csv >> ../yada.csv
#./run_memcache.sh mutex pmdk
/run_memcache.py pmdk
cat memcached-pmdk.csv >> ../memcached.csv
#./run_memcache.sh mutex clobber
/run_memcache.py clobber
cat memcached-clobber.csv >> ../memcached.csv

# run memcached and vacation with mnemosyne
cd ../mnemosyne-gcc/usermode/
./run_vacation.sh
cat vacation.csv >> ../../vacation.csv
#./run_memcache.sh
/run_memcache.py mnemosyne
cat memcached-mnemosyne.csv >> ../../memcached.csv
cd ../..

