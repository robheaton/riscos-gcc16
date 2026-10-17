/* Test-only shim for the GMP/MPFR/MPC tests (not part of the SDK): RISC OS has the dot as the directory separator, so a Unix file name like "io.tmp" names the file "tmp" in the directory "io".
   The names are mapped (. -> _ and / -> .) in fopen and freopen; the data files of the tests are shipped under the mapped names. */
#ifndef ROFILE_H
#define ROFILE_H
#include <stdio.h>
#include <string.h>
static inline const char *ro_name (const char *n, char *buf)
{
  char *p = buf;
  if (!n || !strcmp (n, "/dev/null") || strlen (n) > 400) return n;
  if (!strncmp (n, "/home/", 6)) { const char *t = strstr (n, "/tests/"); if (t) n = t + 7; }	/* the build-time source directory of the MPFR tests (SRCDIR): the data files are in the current directory */
  if (!strncmp (n, "./", 2)) n += 2;										/* MPC: $srcdir is "." */
  for (; *n; n++) *p++ = *n == '.' ? '_' : *n == '/' ? '.' : *n;
  *p = 0;
  return buf;
}
static inline FILE *ro_fopen (const char *n, const char *m) { char b[512]; return fopen (ro_name (n, b), m); }
static inline FILE *ro_freopen (const char *n, const char *m, FILE *f) { char b[512]; return freopen (ro_name (n, b), m, f); }
#define fopen ro_fopen
#define freopen ro_freopen
#endif
