/* <uchar.h> (C11): the types of the 16 and 32 bit characters.  The conversion functions (mbrtoc16 ...) are not in the library: a module has no locale. */
#ifndef _MODLIB_UCHAR_H
#define _MODLIB_UCHAR_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#else
typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;
#endif
#ifdef __cplusplus
}
#endif
#endif
