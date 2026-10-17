/* wio.c - wide character input and output on streams as POSIX has it: the wide characters are converted to and from the multibyte encoding of the locale (picolibc writes and reads the raw wchar_t bytes of a
   "wide stream" instead, which is not what programs expect).  fputwc, putwc, putwchar, fputws, fgetwc, getwc, getwchar, fgetws, ungetwc and fwide.  The SDK of RISC OS 64. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define MBBUF 16								/* (limits.h of picolibc says MB_LEN_MAX is 1, whatever the locale: a character may take up to 6 bytes) */

wint_t fputwc (wchar_t c, FILE *f)
{
  char b[MBBUF];
  mbstate_t ps;
  size_t n, i;
  memset (&ps, 0, sizeof ps);
  n = wcrtomb (b, c, &ps);
  if (n == (size_t) -1) { errno = EILSEQ; return WEOF; }
  for (i = 0; i < n; i++) if (fputc ((unsigned char) b[i], f) == EOF) return WEOF;
  return (wint_t) c;
}
wint_t putwc (wchar_t c, FILE *f) { return fputwc (c, f); }
wint_t putwchar (wchar_t c) { return fputwc (c, stdout); }

int fputws (const wchar_t *s, FILE *f)
{
  for (; *s; s++) if (fputwc (*s, f) == WEOF) return -1;
  return 0;
}

wint_t fgetwc (FILE *f)
{
  mbstate_t ps;
  size_t n = 0;
  memset (&ps, 0, sizeof ps);
  for (;;)
    {
      int c = fgetc (f);
      char ch;
      wchar_t w;
      size_t r;
      if (c == EOF) { if (n) errno = EILSEQ; return WEOF; }
      ch = (char) c;
      n++;
      r = mbrtowc (&w, &ch, 1, &ps);			/* one byte at a time: the state remembers the bytes so far */
      if (r == (size_t) -2) { if (n < MBBUF) continue; errno = EILSEQ; return WEOF; }	/* incomplete: read more */
      if (r == (size_t) -1) { errno = EILSEQ; return WEOF; }
      return (wint_t) w;
    }
}
wint_t getwc (FILE *f) { return fgetwc (f); }
wint_t getwchar (void) { return fgetwc (stdin); }

wchar_t *fgetws (wchar_t *s, int n, FILE *f)
{
  int i = 0;
  if (n <= 0) return 0;
  while (i < n - 1)
    {
      wint_t c = fgetwc (f);
      if (c == WEOF) { if (i == 0) return 0; break; }
      s[i++] = (wchar_t) c;
      if (c == L'\n') break;
    }
  s[i] = 0;
  return s;
}

wint_t ungetwc (wint_t c, FILE *f)
{
  char b[MBBUF];
  mbstate_t ps;
  size_t n, i;
  if (c == WEOF) return WEOF;
  memset (&ps, 0, sizeof ps);
  n = wcrtomb (b, (wchar_t) c, &ps);
  if (n == (size_t) -1) return WEOF;
  for (i = n; i > 0; i--) if (ungetc ((unsigned char) b[i - 1], f) == EOF) return WEOF;	/* (the library guarantees one byte of push-back: enough for one-byte characters) */
  return c;
}

int fwide (FILE *f, int mode) { (void) f; return mode; }
