#pragma once
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <semaphore.h>

#include "shard.h"
#include "util.h"

struct requester_args_t {
	shard *requests; // shared array for unresolved names
	FILE* namefile; // thread-specific file to read names from
	FILE* serviced; // global file to log to
	pthread_mutex_t* serviced_lk; // lock for the above
	sem_t req_sem; // semaphore to request new file
	sem_t resp_sem; // semaphore to wait on the master thread's response
};
typedef struct requester_args_t requester_args;

struct resolver_args_t {
	shard *requests; // shared array for unresolved names
	shard *resolved; // shared array for names + IPs
};
typedef struct resolver_args_t resolver_args;


void* requester_thread (void*);
void* resolver_thread (void*);

