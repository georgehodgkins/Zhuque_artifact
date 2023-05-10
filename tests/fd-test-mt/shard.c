#include "shard.h"

// initializes the shared array object (must already be allocated)
// note that the file pointers need to be initialized separately
void shard_init(shard *s) {
	// intialize array
	for (int a = 0; a < ARRAY_SIZE; ++a) {
		s->array[a] = NULL;
	}

	// initialize counters
	s->p = 0;
	s->g = 0;
	s->terminate = 0;

	// initialize synchronization objects
	int e = 0;
	
	e += pthread_mutex_init(&s->p_lk, NULL);
	e += pthread_mutex_init(&s->g_lk, NULL);
	e += pthread_mutex_init(&s->t_lk, NULL);
	
	e += pthread_cond_init(&s->p_cond, NULL);
	e += pthread_cond_init(&s->g_cond, NULL);

	assert(e == 0 && "Error initializing sync objects!");

}

// put an entry into the array
void shard_put(shard *s, char *entry) {

	// increment put index, blocking if array is full
	pthread_mutex_lock(&s->p_lk);
	// put index on non null entry means array is full
	while(s->array[s->p]) { 
#ifndef NDEBUG
		fprintf(stderr, "Th 0x%lx: blocking on full array\n", (size_t) pthread_self());
#endif
		pthread_cond_wait(&s->p_cond, &s->p_lk);
	}
	int p_loc = s->p++;
	if (s->p == ARRAY_SIZE) s->p = 0; // wrap around
	pthread_mutex_unlock(&s->p_lk);

	// put entry (uncontended)
	assert(s->array[p_loc] == NULL && "Overwriting entry!");
	s->array[p_loc] = entry;

	// wake up a resolver thread if one is blocked
	pthread_cond_signal(&s->g_cond);
}

// retrieve an entry from the array
char* shard_get(shard *s) {

	// increment get index, blocking if array is empty
	pthread_mutex_lock(&s->g_lk);
	// get index on null entry means array is empty
	while (!s->array[s->g]) {
		if (shard_getterm(s)) {
			pthread_mutex_unlock(&s->g_lk);
			return NULL;
		}
#ifndef NDEBUG
		fprintf(stderr, "Th 0x%lx: blocking on empty array\n", (size_t) pthread_self());
#endif
		pthread_cond_wait(&s->g_cond, &s->g_lk);
	}
	int g_loc = s->g++;
	if (s->g == ARRAY_SIZE) s->g = 0; // wrap around
	pthread_mutex_unlock(&s->g_lk);

	// get entry (uncontended)
	assert(s->array[g_loc] != NULL && "Getting empty entry!");
	char *entry = s->array[g_loc];
	s->array[g_loc] = NULL;// clear entry

	// wake up a requester thread if one is blocked
	pthread_cond_signal(&s->p_cond);

	return entry;
}

// non-blocking get
// returns null if no get
char* shard_tryget(shard* s) {

	pthread_mutex_lock(&s->g_lk);
	
	if (!s->array[s->g]) { // array is empty
		pthread_mutex_unlock(&s->g_lk);
		return NULL;
	}

	int g_loc = s->g++;
	if (s->g == ARRAY_SIZE) s->g = 0;
	pthread_mutex_unlock(&s->g_lk);

	assert(s->array[g_loc] != NULL);
	char *entry = s->array[g_loc];
	s->array[g_loc] = NULL;

	pthread_cond_signal(&s->p_cond);

	return entry;
}


// tell gets to stop blocking on empty and return NULL
void shard_terminate(shard* s) {
	pthread_mutex_lock(&s->t_lk);
	s->terminate++;
	pthread_mutex_unlock(&s->t_lk);
	pthread_cond_broadcast(&s->g_cond);
}

int shard_getterm(shard* s) {
	pthread_mutex_lock(&s->t_lk);
	int term = s->terminate;
	pthread_mutex_unlock(&s->t_lk);
	return term;
}

