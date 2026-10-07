/* ctype.c - the functions of <ctype.h> as ordinary functions of libmodkit.a ("C" locale, ASCII).  The header has them inline for the code that includes it; these are for code that declares them itself or
   takes their address.  The header is left out on purpose: its inline definitions would be these functions' names. */
int isdigit (int c) { return c >= '0' && c <= '9'; }
int isupper (int c) { return c >= 'A' && c <= 'Z'; }
int islower (int c) { return c >= 'a' && c <= 'z'; }
int isalpha (int c) { return isupper (c) || islower (c); }
int isalnum (int c) { return isalpha (c) || isdigit (c); }
int isspace (int c) { return c == ' ' || (c >= 9 && c <= 13); }
int isblank (int c) { return c == ' ' || c == 9; }
int iscntrl (int c) { return (c >= 0 && c < 32) || c == 127; }
int isprint (int c) { return c >= 32 && c < 127; }
int isgraph (int c) { return c > 32 && c < 127; }
int ispunct (int c) { return isgraph (c) && !isalnum (c); }
int isxdigit (int c) { return isdigit (c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int isascii (int c) { return c >= 0 && c < 128; }
int toascii (int c) { return c & 0x7F; }
int toupper (int c) { return islower (c) ? c - 32 : c; }
int tolower (int c) { return isupper (c) ? c + 32 : c; }
