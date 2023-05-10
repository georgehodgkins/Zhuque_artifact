#define _BSD_SOURCE
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include "syscall.h"
#include "psys.h"

void *sbrk(intptr_t inc)
{
	if (inc) return (void *)__syscall_ret(-ENOMEM);
	return _p_sbrk(inc);
}
