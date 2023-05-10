#include <sys/socket.h>
#include "syscall.h"
#include "psys.h"

int listen(int fd, int backlog)
{
	return _p_listen(fd, backlog);
}
