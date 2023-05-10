#include <unistd.h>
#include <errno.h>
#include "aio_impl.h"
#include "syscall.h"
#include "psys.h"

static int dummy(int fd)
{
	return fd;
}

weak_alias(dummy, __aio_close);

int close(int fd)
{
	fd = __aio_close(fd);
	return _p_close(fd);
}
