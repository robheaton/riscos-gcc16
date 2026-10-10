/* wchar.h - only what the headers of the C++ library need: the types mbstate_t and wint_t and WEOF.  There are no wide character functions in the C library of the kit (wchar_t is the compiler's: 4 bytes on this
   target); a program that uses wcslen and the like gets an error at the compile (the functions are not declared). */
#ifndef _WCHAR_H
#define _WCHAR_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
typedef unsigned int wint_t;
typedef struct { int __fill[6]; } mbstate_t;
#define WEOF 0xFFFFFFFFu
#ifndef WCHAR_MIN
#define WCHAR_MIN __WCHAR_MIN__
#define WCHAR_MAX __WCHAR_MAX__
#endif
#ifdef __cplusplus
}
#endif
#endif
