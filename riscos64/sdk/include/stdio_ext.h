/* stdio_ext.h - the glibc calls that ask a stream about itself (gnulib and GNU programs use them).  picolibc's streams do not keep the information, so the answers are the cautious ones.  The SDK of RISC OS 64. */
#ifndef _STDIO_EXT_H
#define _STDIO_EXT_H
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FSETLOCKING_QUERY 0
#define FSETLOCKING_INTERNAL 1
#define FSETLOCKING_BYCALLER 2
size_t __fbufsize (FILE *);
size_t __fpending (FILE *);		/* the bytes waiting to be written: 0 (picolibc writes through) */
int __flbf (FILE *);
int __freadable (FILE *);
int __fwritable (FILE *);
int __freading (FILE *);
int __fwriting (FILE *);
int __fsetlocking (FILE *, int);
void _flushlbf (void);
void __fpurge (FILE *);
#ifdef __cplusplus
}
#endif
#endif
