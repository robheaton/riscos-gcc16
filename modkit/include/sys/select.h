/* sys/select.h - select () on descriptors: the sockets among them are asked from the Internet module; a file, the screen and the keyboard are always ready to read and write (and never for exceptions). */
#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H
#include <sys/types.h>
#include <sys/time.h>
#ifdef __cplusplus
extern "C" {
#endif
/* The number of descriptors in the sets that are ready (0: the time ran out), or -1 with errno set.  NFDS is the highest descriptor number plus one. */
extern int select (int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
#ifdef __cplusplus
}
#endif
#endif
