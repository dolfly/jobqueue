#ifndef _SUPPORT_H_
#define _SUPPORT_H_

int closeonexec(int fd);
int pipe_closeonexec(int p[2]);

#endif
