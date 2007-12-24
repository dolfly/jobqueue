#include <unistd.h>
#include <fcntl.h>

#include "support.h"

int closeonexec(int fd)
{
	long flags = fcntl(fd, F_GETFD);

	if (flags == -1)
		return -1;

	return fcntl(fd, F_SETFD, FD_CLOEXEC | flags);
}



int pipe_closeonexec(int p[2])
{
	if (pipe(p))
		return -1;

	if (closeonexec(p[0]))
		return -1;

	return closeonexec(p[1]);
}
