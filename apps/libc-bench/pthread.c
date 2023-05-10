#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

void *emptyfunc(void *dummy)
{
	return 0;
}

size_t b_pthread_createjoin_serial1(void *dummy)
{
	size_t i;
	pthread_t td;
	for (i=0; i<2500; i++) {
		pthread_create(&td, 0, emptyfunc, 0);
		pthread_join(td, &dummy);
	}
	return 0;
}

size_t b_pthread_createjoin_serial2(void *dummy)
{
	size_t i, j;
	pthread_t td[50];
	for (j=0; j<50; j++) {
		for (i=0; i<sizeof td/sizeof *td; i++)
			pthread_create(td+i, 0, emptyfunc, 0);
		for (i=0; i<sizeof td/sizeof *td; i++)
			pthread_join(td[i], &dummy);
	}
	return 0;
}

size_t b_pthread_create_serial1(void *dummy)
{
	size_t i;
	pthread_attr_t attr;
	pthread_t td;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 16384);
	for (i=0; i<2500; i++)
		pthread_create(&td, &attr, emptyfunc, 0);
	return 0;
}

void *lockunlock(void *mut)
{
	size_t i;
	for (i=0; i<1000000; i++) {
		pthread_mutex_lock(mut);
		pthread_mutex_unlock(mut);
	}
	return 0;
}

size_t b_pthread_uselesslock(void *dummy)
{
	pthread_t td;
	pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
	pthread_create(&td, 0, lockunlock, &mut);
	pthread_join(td, &dummy);
	return 0;
}

size_t b_pthread_createjoin_minimal1(void *dummy)
{
	size_t i;
	pthread_t td;
	pthread_attr_t a;
	pthread_attr_init(&a);
	pthread_attr_setstacksize(&a, sysconf(_SC_PAGESIZE));
	pthread_attr_setguardsize(&a, 0);
	for (i=0; i<2500; i++) {
		pthread_create(&td, &a, emptyfunc, 0);
		pthread_join(td, &dummy);
	}
	return 0;
}

size_t b_pthread_createjoin_minimal2(void *dummy)
{
	size_t i, j;
	pthread_t td[50];
	pthread_attr_t a;
	pthread_attr_init(&a);
	pthread_attr_setstacksize(&a, sysconf(_SC_PAGESIZE));
	pthread_attr_setguardsize(&a, 0);
	for (j=0; j<50; j++) {
		for (i=0; i<sizeof td/sizeof *td; i++)
			pthread_create(td+i, &a, emptyfunc, 0);
		for (i=0; i<sizeof td/sizeof *td; i++)
			pthread_join(td[i], &dummy);
	}
	return 0;
}
