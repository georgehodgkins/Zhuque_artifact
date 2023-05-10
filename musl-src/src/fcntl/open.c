#include <fcntl.h>
#include <stdarg.h>
#include "syscall.h"
#include "psys.h"

int open(const char *filename, int flags, ...)
{
	mode_t mode = 0;

	if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}

	return _p_openat(AT_FDCWD, filename, flags, mode);
}

weak_alias(open, open64);
