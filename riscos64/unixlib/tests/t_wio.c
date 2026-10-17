/* t_wio.c - wide characters on streams: converted to UTF-8 on output and back on input (the first locale of the list that setlocale accepts). */
#define _GNU_SOURCE 1
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

int main (void)
{
  static const char *const names[] = { "C.UTF-8", "en_GB.UTF-8", "en_US.UTF-8", 0 };
  int i;
  FILE *f;
  wchar_t w[40];
  unsigned char b[64];
  size_t n;
  wint_t c;
  for (i = 0; names[i]; i++) if (setlocale (LC_ALL, names[i])) break;
  printf ("locale %s, MB_CUR_MAX=%d\n", names[i] ? "UTF-8" : "none", (int) MB_CUR_MAX);
  f = fopen ("wiotest", "w");
  fputws (L"café €", f);
  fputwc (L'\n', f);
  fclose (f);
  f = fopen ("wiotest", "rb");
  n = fread (b, 1, sizeof b, f);
  fclose (f);
  printf ("bytes (%zu):", n);
  for (i = 0; i < (int) n; i++) printf (" %02x", b[i]);
  printf ("\n");
  f = fopen ("wiotest", "r");
  if (fgetws (w, 40, f)) { printf ("fgetws: %zu wide characters:", wcslen (w)); for (i = 0; w[i]; i++) printf (" %x", (unsigned) w[i]); printf ("\n"); }
  rewind (f);
  c = fgetwc (f); printf ("fgetwc: %x\n", (unsigned) c);
  ungetwc (c, f); c = fgetwc (f); printf ("after ungetwc: %x\n", (unsigned) c);
  fclose (f);
  remove ("wiotest");
  fwprintf (stdout, L"fwprintf: %ls %d\n", L"wé", 7);
  return 0;
}
