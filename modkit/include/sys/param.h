/* sys/param.h - the BSD constants and macros that GCCSDK's UnixLib has in <sys/param.h> (the names are those of UnixLib; NBBY is in <sys/types.h>). */
#ifndef _SYS_PARAM_H
#define _SYS_PARAM_H
#ifdef __cplusplus
extern "C" {
#endif
#include <limits.h>
#include <sys/types.h>

#ifndef MAXPATHLEN
#define MAXPATHLEN _POSIX_PATH_MAX              /* 1024 */
#endif
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

#ifndef howmany
#define howmany(x, y) (((x) + ((y) - 1)) / (y))
#endif
#ifndef powerof2
#define powerof2(x) ((((x) - 1) & (x)) == 0)
#endif
#ifndef rounddown
#define rounddown(x, y) (((x) / (y)) * (y))
#endif
#ifndef roundup
#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#endif
#ifndef roundup2
#define roundup2(x, y) (((x) + (y) - 1) & ~((y) - 1))
#endif
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifdef __cplusplus
}
#endif
#endif
