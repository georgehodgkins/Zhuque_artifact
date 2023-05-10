#include <sys/mman.h>
#include "syscall.h"
#include "psys.h"

int __madvise(void *addr, size_t len, int advice)
{
	return _p_madvise(addr, len, advice);
}

weak_alias(__madvise, madvise);
