/* errno.h - the BSD / UnixLib numbers (the Socket module returns them, with a marker bit, in the error numbers of its errors). */
#ifndef _ERRNO_H
#define _ERRNO_H
extern int errno;
#define EPERM 1
#define ENOENT 2
#define EINTR 4
#define EIO 5
#define E2BIG 7
#define EBADF 9
#define ENOMEM 12
#define EACCES 13
#define EBUSY 16
#define EEXIST 17
#define EXDEV 18
#define ENOTDIR 20
#define EISDIR 21
#define EINVAL 22
#define EMFILE 24
#define EFBIG 27
#define ENOSPC 28
#define ESPIPE 29
#define EROFS 30
#define EPIPE 32
#define EDOM 33
#define ERANGE 34
#define EAGAIN 35
#define EWOULDBLOCK EAGAIN
#define EINPROGRESS 36
#define ENAMETOOLONG 63
#define ENOTEMPTY 66
#define ECONNRESET 54
#define EOVERFLOW 91
#define ENOTCONN 57
#endif
