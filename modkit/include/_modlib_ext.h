/* _modlib_ext.h - defines __MODLIB_EXT when the names that are not in the C standard (POSIX, BSD and GNU functions) are to be declared: not with -std=c99 / -ansi, unless asked for
   (_GNU_SOURCE, _DEFAULT_SOURCE, _POSIX_C_SOURCE, _BSD_SOURCE), so that a program that defines getline or strdup itself has no clash. */
#ifndef __MODLIB_EXT
#if !defined (__STRICT_ANSI__) || defined (_GNU_SOURCE) || defined (_DEFAULT_SOURCE) || defined (_POSIX_C_SOURCE) || defined (_BSD_SOURCE)
#define __MODLIB_EXT 1
#endif
#endif
