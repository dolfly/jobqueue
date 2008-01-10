#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>

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


/* Read a line from file 'f' to buffer 'buf' with size 'buflen', and
 * strip \n away
 */
ssize_t read_stripped_line(char *buf, size_t buflen, FILE *f)
{
	size_t len;

	while (fgets(buf, buflen, f) == NULL) {
		/* fgets() may be unable to restart after SIGCHLD:
		   it can return NULL, and feof() needs to be tested */
		if (feof(f))
			return -1;
	}

	len = strlen(buf);

	if (buf[len - 1] == '\n') {
		len--;
		buf[len] = 0;
	}

	return len;
}


/* Return 0, if:
 *    a) line is empty (len == 0)
 *    b) line has only whitespace characters
 *    c) line begins with #
 *
 * Otherwise return 1.
 */
int useful_line(const char *buf)
{
	size_t i;
	size_t len = strlen(buf);

	/* Ignore empty lines and lines commented with # */
	if (len == 0 || buf[0] == '#')
		return 0;

	/* Ignore whitespace lines */
	for (i = 0; i < len; i++) {
		if (!isspace(buf[i]))
			return 1;
	}

	return 0;
}
