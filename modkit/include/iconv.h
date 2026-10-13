/* iconv.h - iconv_open, iconv and iconv_close for UTF-8, ASCII (US-ASCII), ISO-8859-1 (LATIN1), UTF-16LE and BE, UTF-32LE and BE (UCS-4LE and BE, WCHAR_T is UTF-32LE): any of them to any other.  The names
   are not case sensitive.  Stateless encodings only: there is no UTF-16 or UTF-32 without a byte order (no BOM), no UCS-2 and no stateful encoding such as ISO-2022; iconv_open of another name gives EINVAL.  A suffix
   //TRANSLIT or //IGNORE is accepted and ignored.  The rules are glibc's: a sequence that is not valid gives EILSEQ, one that is cut short at the end of the input gives EINVAL, a full output E2BIG; the pointers say
   how far the conversion got, and the result is the number of irreversible conversions, which is 0 here.  Surrogates, overlong forms and values above U+10FFFF are not valid. */
#ifndef _ICONV_H
#define _ICONV_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void *iconv_t;
extern iconv_t iconv_open (const char *tocode, const char *fromcode);
extern size_t iconv (iconv_t cd, char **inbuf, size_t *inbytesleft, char **outbuf, size_t *outbytesleft);
extern int iconv_close (iconv_t cd);
#ifdef __cplusplus
}
#endif
#endif
