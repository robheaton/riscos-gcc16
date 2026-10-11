/* utime.h - utime (): set the modification time of a file (its RISC OS date stamp; the object must have one). */
#ifndef _UTIME_H
#define _UTIME_H
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
struct utimbuf { time_t actime; time_t modtime; };
extern int utime (const char *path, const struct utimbuf *times);        /* NULL is now; the access time is not kept */
#ifdef __cplusplus
}
#endif
#endif
