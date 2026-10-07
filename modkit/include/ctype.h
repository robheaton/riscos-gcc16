/* ctype.h - the classification and case functions of the "C" locale (ASCII), as inline functions: a module that only includes this needs nothing from the library.  ctype.c has the same functions
   as ordinary ones (for code that declares them itself or takes their address through another header).  EOF (-1) and any value that is not a character is "no" for every class. */
#ifndef _CTYPE_H
#define _CTYPE_H
static inline int isdigit (int c) { return c >= '0' && c <= '9'; }
static inline int isupper (int c) { return c >= 'A' && c <= 'Z'; }
static inline int islower (int c) { return c >= 'a' && c <= 'z'; }
static inline int isalpha (int c) { return isupper (c) || islower (c); }
static inline int isalnum (int c) { return isalpha (c) || isdigit (c); }
static inline int isspace (int c) { return c == ' ' || (c >= 9 && c <= 13); }
static inline int isblank (int c) { return c == ' ' || c == 9; }
static inline int iscntrl (int c) { return (c >= 0 && c < 32) || c == 127; }
static inline int isprint (int c) { return c >= 32 && c < 127; }
static inline int isgraph (int c) { return c > 32 && c < 127; }
static inline int ispunct (int c) { return isgraph (c) && !isalnum (c); }
static inline int isxdigit (int c) { return isdigit (c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static inline int isascii (int c) { return c >= 0 && c < 128; }
static inline int toascii (int c) { return c & 0x7F; }
static inline int toupper (int c) { return islower (c) ? c - 32 : c; }
static inline int tolower (int c) { return isupper (c) ? c + 32 : c; }
#endif
