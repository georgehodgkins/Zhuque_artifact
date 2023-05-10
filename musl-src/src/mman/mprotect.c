#include <sys/mman.h>
#include "libc.h"
#include "syscall.h"

#include "psys.h"

int __mprotect(void *addr, size_t len, int prot)
{
	/*size_t start, end;
	start = (size_t)addr & -PAGE_SIZE;
	end = (size_t)((char *)addr + len + PAGE_SIZE-1) & -PAGE_SIZE;
	return syscall(SYS_mprotect, start, end-start, prot);*/
#ifdef TIME_PSYS
	TIMER_START();
	int s = _p_mprotect(addr, len, prot);
	TIMER_STOP("mprotect()");
	return s;
#else
	return _p_mprotect(addr, len, prot);
#endif
}

weak_alias(__mprotect, mprotect);
