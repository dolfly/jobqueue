#ifndef _SUPPORT_H_
#define _SUPPORT_H_

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define die(fmt, args...) do { \
    fprintf(stderr, fmt, ## args); \
    exit(1); \
  } while (0)

#define dieerror(fmt, args...) do { \
	fprintf(stderr, fmt ": %s\n", ## args, strerror(errno)); \
	exit(1); \
} while(0)


void can_not_open_file(const char *fname);

int closeonexec(int fd);
int pipe_closeonexec(int p[2]);

ssize_t read_stripped_line(char *buf, size_t buflen, FILE *f);

int skipnws(const char *s, int i);
int skipws(const char *s, int i);

int useful_line(const char *buf);

#endif
