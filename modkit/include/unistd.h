/* unistd.h - the few POSIX functions of <unistd.h> that a module has: the host and domain name.  No descriptor functions (open, read, write ...) and no process functions: a module has FILE streams only. */
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
extern int gethostname (char *name, size_t len);                           /* Inet$HostName (with Inet$LocalDomain added when it has no dot), else "acorn" and the station number */
extern int getdomainname (char *name, size_t len);                         /* Inet$LocalDomain, else what follows the first dot of Inet$HostName, else ENOSYS */
#ifdef __cplusplus
}
#endif
#endif
