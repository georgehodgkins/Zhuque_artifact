#include <unistd.h>
#include "syscall.h"
#include "psys.h"

int pipe(int fd[2])
{
	return _p_pipe2(fd, 0);
}
