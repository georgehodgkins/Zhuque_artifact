#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>

#include "syscall.h"
#include "psys.h"


void *__mmap(void *start, size_t len, int prot, int flags, int fd, off_t off)
{
#ifdef TIME_PSYS
	TIMER_START();
	void* addr = _p_mmap(start, len, prot, flags, fd, off);
	TIMER_STOP("mmap()");
	return addr;
#else
	return _p_mmap(start, len, prot, flags, fd, off);
#endif
}

weak_alias(__mmap, mmap);

weak_alias(mmap, mmap64);
