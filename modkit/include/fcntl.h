/* fcntl.h - open (), creat () and fcntl () and their constants as GCCSDK's UnixLib has them (a module that serves UnixLib programs, ARMEABISupport, uses them). */
#ifndef _FCNTL_H
#define _FCNTL_H
#include <sys/types.h>
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_ACCMODE 3
#define O_CREAT 0x0200
#define O_EXCL 0x0800
#define O_TRUNC 0x0400
#define O_ASYNC 0x0040
#define O_FSYNC 0x0080
#define O_SYNC O_FSYNC
#define O_SHLOCK 0x0010
#define O_EXLOCK 0x0020
#define O_APPEND 0x0008
#define O_NONBLOCK 0x0004
#define O_NDELAY O_NONBLOCK
#define O_EXECCL 0x0100
#define O_CLOEXEC O_EXECCL
#define O_NOCTTY 0x10000
#define O_LARGEFILE 0x20000
#define O_BINARY 0
#define O_TEXT 0
#define F_DUPFD 0
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define F_GETOWN 5
#define F_SETOWN 6
#define F_GETLK 7
#define F_SETLK 8
#define F_SETLKW 9
#define FD_CLOEXEC O_EXECCL
#define F_RDLCK 1
#define F_UNLCK 2
#define F_WRLCK 3
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#ifdef __cplusplus
extern "C" {
#endif
extern int open (const char *path, int flags, ...);                        /* a file (not a directory): the descriptor is 3 or more; the mode is ignored (RISC OS has no permission bits) */
extern int open64 (const char *path, int flags, ...);
extern int creat (const char *path, mode_t mode);
extern int creat64 (const char *path, mode_t mode);
/* F_DUPFD (the lowest free descriptor of the argument or more), F_GETFD and F_SETFD (there is no close on exec: always 0), F_GETFL (the access mode, O_APPEND and O_NONBLOCK) and F_SETFL (O_NONBLOCK of a
   socket: FIONBIO; the other flags cannot be changed).  Other commands: EINVAL. */
extern int fcntl (int fd, int cmd, ...);
#ifdef __cplusplus
}
#endif
#endif
