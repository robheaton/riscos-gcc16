/* wctype.h - only the types, for the headers of the C++ library (libstdc++ is configured without wide characters on this target: its <cwctype> includes this header and declares nothing from it).  There
   are no wide character functions in the C library of the kit. */
#ifndef _WCTYPE_H
#define _WCTYPE_H
#include <wchar.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int wctype_t;
typedef int wctrans_t;
#ifdef __cplusplus
}
#endif
#endif
