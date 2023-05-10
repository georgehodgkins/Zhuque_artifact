#include <sys/socket.h>
#include "syscall.h"
#include "psys.h"

int bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	return _p_bind(fd, addr, len);
}
