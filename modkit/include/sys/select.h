/* sys/select.h - select () on sockets (the sockets of the Internet module, not files). */
#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H
#include <sys/types.h>
#include <sys/time.h>
#ifdef __cplusplus
extern "C" {
#endif
/* The number of sockets in the sets that are ready (0: the time ran out), or -1 with errno set.  NFDS is the highest socket number plus one. */
extern int select (int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
#ifdef __cplusplus
}
#endif
#endif
