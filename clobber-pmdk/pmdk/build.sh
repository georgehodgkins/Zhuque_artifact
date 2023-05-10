#!/bin/sh
PMDK_VER='1.6'
set -e
cd src

make -j15 libpmemobj
if [ $? -ne 0 ]; then
    echo 'Unable to make PMDK'
    exit 1
fi

make install -j15
if [ $? -ne 0 ]; then
    echo 'Unable to install PMDK!'
    exit 1
fi
cd ..

exit 0
