/* unistd.h - the POSIX functions of <unistd.h> that a module has: the descriptor functions on files (read, write, close, lseek, isatty, unlink, ftruncate, truncate; open is in <fcntl.h>) and the host and domain name.  No process functions. */
#ifndef _UNISTD_H
#define _UNISTD_H
#include <stddef.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
#define STDIN_FILENO	0
#define STDOUT_FILENO	1
#define STDERR_FILENO	2
#ifndef SEEK_SET
#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2
#endif
extern ssize_t read (int fd, void *buf, size_t n);                         /* the descriptors are those of open () and fileno (): files and the standard streams; not sockets (see <sys/socket.h>) */
extern ssize_t write (int fd, const void *buf, size_t n);
extern int close (int fd);
extern off_t lseek (int fd, off_t offset, int whence);
extern off64_t lseek64 (int fd, off64_t offset, int whence);
#define F_OK		0
#define R_OK		4
#define W_OK		2
#define X_OK		1
extern int access (const char *path, int mode);                            /* the bits of stat ()'s mode: a file that is not Text or Data is executable when it is readable, as in UnixLib */
extern int dup (int fd);                                                   /* a second descriptor of the same file (the same position); closing one leaves the other */
extern int dup2 (int fd, int fd2);                                         /* fd2 from 3 up: what it was is closed; 0 to 2 cannot be redirected (ENOSYS) */
extern int fsync (int fd);                                                 /* what is buffered is written, then OS_Args 255 */
extern int fdatasync (int fd);
extern int isatty (int fd);
extern int unlink (const char *path);                                      /* a file, not a directory (EISDIR) */
extern int ftruncate (int fd, off_t length);
extern int truncate (const char *path, off_t length);
extern int gethostname (char *name, size_t len);                           /* Inet$HostName (with Inet$LocalDomain added when it has no dot), else "acorn" and the station number */
extern int getdomainname (char *name, size_t len);                         /* Inet$LocalDomain, else what follows the first dot of Inet$HostName, else ENOSYS */
#ifdef __cplusplus
}
#endif
#endif
