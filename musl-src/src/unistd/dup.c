#include <unistd.h>
#include "syscall.h"
#include "psys.h"

int dup(int fd)
{
	return _p_dup(fd);
}
