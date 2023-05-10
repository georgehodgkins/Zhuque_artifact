#include <fcntl.h>
#include "psys.h"

int creat(const char *filename, mode_t mode)
{
	return _p_openat(AT_FDCWD, filename, O_CREAT|O_WRONLY|O_TRUNC, mode);
}

weak_alias(creat, creat64);
