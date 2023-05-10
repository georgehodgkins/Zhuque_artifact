/*!
 * \file
 */
#include "pvar.h"
#include <stdio.h>
#include <stdlib.h>
#include <pmalloc.h>
#include <pthread.h>
#include <signal.h>

// Pedro: On my laptop it passes with an array of 937, but not with 962. 
// I'm putting 4000 to make sure it's reproducible  
// Some of the errors:
// bigalloc: /home/vagrant/mnemosyne-gcc/usermode/library/pmalloc/include/alps/src/layers/extentmap.cc:176: void alps::ExtentMap
// ::insert(const alps::ExtentInterval&): Assertion `verify_maplen_equivalent_to_mapaddr() == true' failed.
// Aborted (core dumped)
//
// Another possible error (array of 1000):
// Floating point exception (core dumped)
typedef struct {
    long long a[1000];
} myarray_t;

unsigned long long cl_mask = 0xffffffffffffffc0;
#define sz 32
#define count 100
long *ptr;
pthread_t thr[2];

void* reader()
{
	printf("(READER) persistent ptr =%p, sz=%d\n", ptr, sz);
	int i = 0;
	long rd;
	while(i++ < count)
	{
		PTx { rd = *ptr; } 	
	}
}

void* writer()
{
	printf("(WRITER) persistent ptr =%p, sz=%d\n", ptr, sz);
	int i = 0;
	long wrt = 1;
	myarray_t *ma;
	while(i++ < count)
	{
		/* Pedro: This crashes on my laptop... am I missing some step? */
		PTx { 
			*ptr = wrt; *ptr = 2; *ptr = 3; *ptr = 4; 
			ma = pmalloc(sizeof(myarray_t)); 
			ma->a[0] = 42;
			pfree(ma); 
		}
	}
}

void malloc_bench()
{
	ptr = NULL;
	PTx { ptr = pmalloc(sz); }
	if(ptr != NULL)
	{
	       printf("persistent ptr =%p, sz=%d\n", ptr, sz);
	       printf("persistent ptr & cl_mask = %p\n", (void*)((unsigned long)ptr & cl_mask));
	       pthread_create(&thr[0], NULL,
                          &writer, NULL);

	       //pthread_create(&thr[1], NULL,
               //           &reader, NULL);
	}

	pthread_join(thr[0], NULL);
	//pthread_join(thr[1], NULL);
}

int main (int argc, char const *argv[])
{
	printf("persistent flag: %d", PGET(flag));

	PTx {
		if (PGET(flag) == 0) {
			PSET(flag, 1);
		} else {
			PSET(flag, 0);
		}
	}
	
	printf(" --> %d\n", PGET(flag));
	printf("&persistent flag = %p\n", PADDR(flag));

	printf("\nstarting malloc bench\n");
	malloc_bench();
	return 0;
}
