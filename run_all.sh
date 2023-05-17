#!/bin/bash
set -e

cd /clobber-pmdk
./run_all.sh

cd ../apps
./run_all.sh

rm -rf /results && mkdir /results
cd /results

# compile results
cat /clobber-pmdk/memcached.csv >> memcached.csv
cat /apps/memcached.csv >> memcached.csv

cp /apps/memcached_new.csv .

cat /clobber-pmdk/vacation.csv >> vacation.csv
cat /apps/vacation.csv >> vacation.csv

cat /clobber-pmdk/yada.csv >> yada.csv
cat /apps/yada.csv >> yada.csv

cp /apps/libc-bench*.out .
cp /apps/sneksit.out .

exit 0
