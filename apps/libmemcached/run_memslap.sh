#!/bin/bash

PWD=`pwd`

MEMCACHED_RUN_CNF="$PWD/memaslap.cnf"

MEMASLAP_BIN=$PWD/clients/memaslap
MEMASLAP_THREADS="4"  
MEMASLAP_CONCURRENCY="4" 

VALUE_SIZE_ARR=( 64)
SERVER_IP="127.0.0.1"
SERVER_PORT=11211

action=$1

if [[ $action == '-h' ]]
then
	$MEMASLAP_BIN -h
else
idx=0
for (( idx=0; idx<${#VALUE_SIZE_ARR[@]}; idx++ ))
do
	val_size=${VALUE_SIZE_ARR[$idx]}
	num_ops=100000
	run_time="10s"
	$MEMASLAP_BIN -s $SERVER_IP:$SERVER_PORT -c $MEMASLAP_CONCURRENCY -x $num_ops -T $MEMASLAP_THREADS -X $val_size -F $MEMCACHED_RUN_CNF -d 1
	#$MEMASLAP_BIN -s $SERVER_IP:$SERVER_PORT -c $MEMASLAP_CONCURRENCY -T $MEMASLAP_THREADS -X $val_size -F $MEMCACHED_RUN_CNF -d 1 -t $run_time
	sleep 1
done
fi

exit

