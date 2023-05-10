#define _BSD_SOURCE
#include <unistd.h>
#include <errno.h>
#include "syscall.h"
#include "psys.h"

int brk(void *end)
{
	return _p_brk(end);
}
