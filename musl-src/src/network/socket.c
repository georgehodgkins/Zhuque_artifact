#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>

#include "syscall.h"
#include "psys.h"

int socket(int domain, int type, int protocol)
{
	return _p_socket(domain, type, protocol);
}
