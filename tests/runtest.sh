#!/bin/bash

set -e
trap '' PWR

cd fd-test-st
../restartit.sh 0.005 ./test

cd ../fd-test-mt
../restartit.sh 5.0 ./multi-lookup 4 4 rqloq.txt rslog.txt input/*

cd ..
