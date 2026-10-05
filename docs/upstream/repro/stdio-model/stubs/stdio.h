/* the little of UnixLib's <stdio.h> that stdio/fread.c and stdio/fwrite.c use.  struct __iobuf is the REAL one: build-and-run.sh cuts it out of include/stdio.h of the
   sources under test into stdio_struct.h. */
#ifndef MODEL_STDIO_H
#define MODEL_STDIO_H
#include <stddef.h>
#include <sys/types.h>
#include <pthread.h>
typedef struct __iobuf FILE;
#include "stdio_struct.h"
#define EOF (-1)
#define _IOMAGIC 0x4f4d4f44u
#define __validfp(stream) ((stream) != NULL && (stream)->__magic == _IOMAGIC)
#define feof(stream) ((stream)->__eof != 0)
#define ferror(stream) ((stream)->__error != 0)
#define fileno(f) ((f)->fd)
extern int __flsbuf (int __c, FILE *__stream);
extern int __flslbbuf (void);
extern size_t fread (void *__data, size_t __size, size_t __count, FILE *__stream);
extern size_t fwrite (const void *__data, size_t __size, size_t __count, FILE *__stream);
#define MODEL_STR2(x) #x
#define MODEL_STR(x) MODEL_STR2 (x)
#define strong_alias(a, b) extern __typeof (a) b __attribute__ ((alias (MODEL_STR (a))));
#endif
