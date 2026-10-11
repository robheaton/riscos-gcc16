/* mbyte.c - multibyte characters in the C locale (the only locale there is): one byte is one character, 0 - 127, and a byte above 127 is an encoding error (EILSEQ), as in glibc's C locale.  The wide
   character is the byte's value; wchar_t is the compiler's (4 bytes, unsigned).  The conversion state has nothing to remember (mbsinit is always true). */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

int mblen (const char *s, size_t n)
{
  if (!s) return 0;
  if (*s == 0) return 0;                                         /* (glibc looks at the byte first: an empty string is 0 whatever N says) */
  if (n == 0) return -1;
  if ((unsigned char) *s > 127) { errno = EILSEQ; return -1; }
  return 1;
}

int mbtowc (wchar_t *pwc, const char *s, size_t n)
{
  if (!s) return 0;
  if (*s == 0) { if (pwc) *pwc = 0; return 0; }
  if (n == 0) return -1;
  if ((unsigned char) *s > 127) { errno = EILSEQ; return -1; }
  if (pwc) *pwc = (unsigned char) *s;
  return 1;
}

int wctomb (char *s, wchar_t wc)
{
  if (!s) return 0;
  if (wc > 127) { errno = EILSEQ; return -1; }
  *s = (char) wc;
  return 1;
}

size_t mbstowcs (wchar_t *dst, const char *src, size_t n)
{
  size_t i = 0;
  if (!dst)
    {
      for (; ; i++)
        {
          if ((unsigned char) src[i] > 127) { errno = EILSEQ; return (size_t) -1; }
          if (!src[i]) return i;
        }
    }
  for (; i < n; i++)
    {
      if ((unsigned char) src[i] > 127) { errno = EILSEQ; return (size_t) -1; }
      dst[i] = (unsigned char) src[i];
      if (!src[i]) return i;
    }
  return i;
}

size_t wcstombs (char *dst, const wchar_t *src, size_t n)
{
  size_t i = 0;
  if (!dst)
    {
      for (; ; i++)
        {
          if (src[i] > 127) { errno = EILSEQ; return (size_t) -1; }
          if (!src[i]) return i;
        }
    }
  for (; i < n; i++)
    {
      if (src[i] > 127) { errno = EILSEQ; return (size_t) -1; }
      dst[i] = (char) src[i];
      if (!src[i]) return i;
    }
  return i;
}

int mbsinit (const mbstate_t *ps) { (void) ps; return 1; }

size_t mbrlen (const char *s, size_t n, mbstate_t *ps) { return mbrtowc (NULL, s, n, ps); }

size_t mbrtowc (wchar_t *pwc, const char *s, size_t n, mbstate_t *ps)
{
  (void) ps;
  if (!s) return 0;
  if (n == 0) return (size_t) -2;
  if ((unsigned char) *s > 127) { errno = EILSEQ; return (size_t) -1; }
  if (pwc) *pwc = (unsigned char) *s;
  return *s != 0;
}

size_t wcrtomb (char *s, wchar_t wc, mbstate_t *ps)
{
  (void) ps;
  if (!s) return 1;
  if (wc > 127) { errno = EILSEQ; return (size_t) -1; }
  *s = (char) wc;
  return 1;
}

size_t mbsrtowcs (wchar_t *dst, const char **src, size_t n, mbstate_t *ps)
{
  size_t i = 0;
  const char *s = *src;
  (void) ps;
  for (; ; i++)
    {
      if (dst && i == n) { *src = s + i; return i; }
      if ((unsigned char) s[i] > 127) { *src = s + i; errno = EILSEQ; return (size_t) -1; }
      if (dst) dst[i] = (unsigned char) s[i];
      if (!s[i]) { if (dst) *src = NULL; return i; }
    }
}

size_t wcsrtombs (char *dst, const wchar_t **src, size_t n, mbstate_t *ps)
{
  size_t i = 0;
  const wchar_t *s = *src;
  (void) ps;
  for (; ; i++)
    {
      if (dst && i == n) { *src = s + i; return i; }
      if (s[i] > 127) { *src = s + i; errno = EILSEQ; return (size_t) -1; }
      if (dst) dst[i] = (char) s[i];
      if (!s[i]) { if (dst) *src = NULL; return i; }
    }
}

wint_t btowc (int c)
{
  if (c == (-1) || c < 0 || c > 127) return WEOF;
  return (wint_t) c;
}

int wctob (wint_t c)
{
  if (c > 127) return (-1);
  return (int) c;
}
