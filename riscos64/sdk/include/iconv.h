#ifndef _ICONV_H
#define _ICONV_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *iconv_t;

extern iconv_t iconv_open (const char *__tocode, const char *__fromcode);
extern size_t iconv (iconv_t __cd, char **__restrict __inbuf, size_t *__restrict __inbytesleft, char **__restrict __outbuf, size_t *__restrict __outbytesleft);
extern int iconv_close (iconv_t __cd);

#ifdef __cplusplus
}
#endif

#endif
