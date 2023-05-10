#include <sys/socket.h>
#include "syscall.h"
#include "psys.h"

int accept(int fd, struct sockaddr *restrict addr, socklen_t *restrict len)
{
	return _p_accept(fd, addr, len);
}
