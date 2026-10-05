/* errno.h - the BSD / UnixLib numbers (the Socket module returns them, with a marker bit, in the error numbers of its errors). */
#ifndef _ERRNO_H
#define _ERRNO_H
extern int errno;
#define ENOENT 2
#define EINTR 4
#define EBADF 9
#define ENOMEM 12
#define EACCES 13
#define EEXIST 17
#define EINVAL 22
#define EPIPE 32
#define EAGAIN 35
#define EWOULDBLOCK EAGAIN
#define EINPROGRESS 36
#define ECONNRESET 54
#define ENOTCONN 57
#endif
