#pragma once
#include <pthread.h>
#include <assert.h>
#include <stdio.h>

#define ARRAY_SIZE 20

struct shard_t { // thread-safe string circular buffer

	// array put index
	int p;
	// array put mutex
	pthread_mutex_t p_lk;
	// array put condvar (blocking when full)
	pthread_cond_t p_cond;

	// array get index
	int g;	
	// array get index mutex
	pthread_mutex_t g_lk;
	// array get condvar (blocking when empty)
	pthread_cond_t g_cond;
	
	// termination indicator (tells shard_get to stop blocking and return NULL)
	int terminate;
	// lock protecting termination indicator
	pthread_mutex_t t_lk;

	// shared array
	char* array[ARRAY_SIZE];
};
typedef struct shard_t shard;

void shard_init(shard*);
void shard_put(shard*, char*);
char* shard_get(shard*);
char* shard_tryget(shard*);
void shard_terminate(shard*);
int shard_getterm(shard*);

