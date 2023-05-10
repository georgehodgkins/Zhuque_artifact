#!/bin/bash
set -e

LOCK=$1
LIB=$2
DIRNAME="memcached-${LOCK}-${LIB}"
cp -r memcached $DIRNAME
cd $DIRNAME

if [ $LIB == 'pmdk' ]; then
{
    echo 'Run PMDK!'
    cp ../runtime/pmdk.o ../runtime/clobber.o
}
else
    echo 'Run Clobber!'
fi

if [ $LOCK == 'mutex' ]; then
    cp thread_mutex.c thread.c
else
    cp thread_rwlock.c thread.c
fi


./configure
make clean
sed -i -e 's:CC = gcc:CC = ../../rollinlineclang:g' Makefile
sed -i -e 's:LIBS =  -levent:LIBS =  -levent -lpthread -lpmemobj:g' Makefile
sed -i -e 's:$(LINK) $(memcached_LDFLAGS) $(memcached_OBJECTS) $(memcached_LDADD):$(LINK) $(memcached_LDFLAGS) $(memcached_OBJECTS) $(memcached_LDADD) ../runtime/clobber.o ../runtime/context.o $(LIBS):g' Makefile
sed -i -e 's:$(LINK) $(memcached_debug_LDFLAGS) $(memcached_debug_OBJECTS) $(memcached_debug_LDADD):$(LINK) $(memcached_debug_LDFLAGS) $(memcached_debug_OBJECTS) $(memcached_debug_LDADD) ../runtime/clobber.o ../runtime/context.o $(LIBS):g' Makefile
sed -i -e 's:CFLAGS = -g -O2:CFLAGS = -g -O2 -pthread -Wl,--wrap=pthread_join -Wl,--wrap=pthread_create -DUSE_THREADS:g' Makefile
make

exit 0