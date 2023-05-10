#include <sys/socket.h>
#include "syscall.h"
#include "psys.h"

int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	return _p_connect(fd, addr, len);
}
