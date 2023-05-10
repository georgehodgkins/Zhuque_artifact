#include <sys/mman.h>
#include "syscall.h"
#include "psys.h"

static void dummy(void) { }
weak_alias(dummy, __vm_wait);

int __munmap(void *start, size_t len)
{
	__vm_wait();
#ifdef TIME_PSYS
	TIMER_START();
	int s = _p_munmap(start, len);
	TIMER_STOP("munmap()");
	return s;
#else
	return _p_munmap(start, len);
#endif

}

weak_alias(__munmap, munmap);
