#ifndef _SUPPORT_H_
#define _SUPPORT_H_

#include <stdio.h>
#include <unistd.h>


#define die(fmt, args...) do { \
    fprintf(stderr, fmt, ## args); \
    exit(1); \
  } while (0)


int closeonexec(int fd);
int pipe_closeonexec(int p[2]);

ssize_t read_stripped_line(char *buf, size_t buflen, FILE *f);

int useful_line(const char *buf);

#endif
