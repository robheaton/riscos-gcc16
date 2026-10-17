/* t_wide.c - what the C library does with locales, multibyte and wide characters and iconv.  It prints results; the host's (glibc, run in the C locale) are the reference: compare with  diff <(host) <(service).  Lines that
   depend on the library on purpose start with "INFO" (setlocale of other locales). */
#define _GNU_SOURCE 1
#include <ctype.h>
#include <iconv.h>
#include <langinfo.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

static void hex (const char *what, const void *p, size_t n)
{
  const unsigned char *b = (const unsigned char *) p;
  size_t i;
  printf ("%s:", what);
  for (i = 0; i < n; i++) printf (" %02x", b[i]);
  printf ("\n");
}

int main (void)
{
  char buf[64];
  wchar_t w[16];
  size_t n;
  mbstate_t st;
  iconv_t cd;
  char in[32], out[64], *ip, *op;
  size_t il, ol;
  struct tm tm;

  printf ("setlocale(LC_ALL,NULL)=%s\n", setlocale (LC_ALL, NULL));
  printf ("setlocale(LC_ALL,\"C\")=%s\n", setlocale (LC_ALL, "C"));
  printf ("INFO setlocale(LC_ALL,\"\")=%s\n", setlocale (LC_ALL, "") ? "ok" : "NULL");
  printf ("INFO setlocale(en_GB.UTF-8)=%s\n", setlocale (LC_ALL, "en_GB.UTF-8") ? "ok" : "NULL");
  setlocale (LC_ALL, "C");
  printf ("CODESET=%s\n", nl_langinfo (CODESET));
  printf ("MB_CUR_MAX=%d\n", (int) MB_CUR_MAX);
  printf ("isalpha(0xe9)=%d toupper(0xe9)=%d\n", isalpha (0xe9) != 0, toupper (0xe9));
  printf ("iswalpha(0xe9)=%d towupper(0xe9)=%x iswupper(0x41)=%d\n", iswalpha (0xe9) != 0, (unsigned) towupper (0xe9), iswupper (0x41) != 0);
  printf ("strcoll(a,B)=%d strcmp=%d\n", strcoll ("a", "B") < 0, strcmp ("a", "B") < 0);
  memset (&st, 0, sizeof st);
  n = mbrtowc (w, "A", 1, &st);
  printf ("mbrtowc(A)=%zu w=%x\n", n, (unsigned) w[0]);
  memset (&st, 0, sizeof st);
  n = mbrtowc (w, "\xc3\xa9", 2, &st);
  printf ("mbrtowc(c3 a9) in C locale=%s w=%x\n", n == 1 ? "1" : n == 2 ? "2" : n == (size_t) -1 ? "-1" : "other", (unsigned) w[0]);
  n = mbstowcs (w, "hello", 16);
  printf ("mbstowcs(hello)=%zu\n", n);
  n = wcstombs (buf, L"hi", 64);
  printf ("wcstombs(hi)=%zu %s\n", n, buf);
  printf ("wcwidth(A)=%d wcwidth(0x4e2d)=%d\n", wcwidth ('A'), wcwidth (0x4e2d));
  printf ("wcslen=%zu wcscmp=%d\n", wcslen (L"abc"), wcscmp (L"abc", L"abd") < 0);
  swprintf (w, 16, L"%d-%ls", 42, L"x");
  printf ("swprintf=%ls\n", w);
  printf ("%ls\n", L"wide printf");
  memset (&tm, 0, sizeof tm); tm.tm_year = 126; tm.tm_mon = 9; tm.tm_mday = 17; tm.tm_hour = 19; tm.tm_min = 20; tm.tm_wday = 6;
  strftime (buf, sizeof buf, "%a %b %d %H:%M %Y", &tm);
  printf ("strftime=%s\n", buf);

  cd = iconv_open ("UTF-8", "ISO-8859-1");
  printf ("iconv_open(UTF-8,ISO-8859-1)=%s\n", cd == (iconv_t) -1 ? "FAIL" : "ok");
  if (cd != (iconv_t) -1)
    {
      memcpy (in, "caf\xe9", 5); ip = in; il = 4; op = out; ol = sizeof out;
      n = iconv (cd, &ip, &il, &op, &ol);
      printf ("iconv=%zd ", (ssize_t) n); hex ("out", out, sizeof out - ol);
      iconv_close (cd);
    }
  cd = iconv_open ("ISO-8859-1", "UTF-8");
  printf ("iconv_open(ISO-8859-1,UTF-8)=%s\n", cd == (iconv_t) -1 ? "FAIL" : "ok");
  if (cd != (iconv_t) -1)
    {
      memcpy (in, "caf\xc3\xa9", 6); ip = in; il = 5; op = out; ol = sizeof out;
      n = iconv (cd, &ip, &il, &op, &ol);
      printf ("iconv=%zd ", (ssize_t) n); hex ("out", out, sizeof out - ol);
      iconv_close (cd);
    }
  cd = iconv_open ("UTF-16LE", "UTF-8");
  printf ("iconv_open(UTF-16LE,UTF-8)=%s\n", cd == (iconv_t) -1 ? "FAIL" : "ok");
  if (cd != (iconv_t) -1)
    {
      memcpy (in, "A\xe2\x82\xac", 4); ip = in; il = 4; op = out; ol = sizeof out;
      n = iconv (cd, &ip, &il, &op, &ol);
      printf ("iconv=%zd ", (ssize_t) n); hex ("out", out, sizeof out - ol);
      iconv_close (cd);
    }
  printf ("INFO iconv_open(UTF-8,CP1252)=%s\n", (cd = iconv_open ("UTF-8", "CP1252")) == (iconv_t) -1 ? "FAIL" : "ok");
  printf ("INFO iconv_open(UTF-8,ISO-8859-15)=%s\n", (cd = iconv_open ("UTF-8", "ISO-8859-15")) == (iconv_t) -1 ? "FAIL" : "ok");
  printf ("INFO iconv_open(UTF-8,UTF-32)=%s\n", (cd = iconv_open ("UTF-8", "UTF-32")) == (iconv_t) -1 ? "FAIL" : "ok");
  return 0;
}
