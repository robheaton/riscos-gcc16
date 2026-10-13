/* modg3.c - stat, access, dup, fsync, <complex.h>, <iconv.h>, the new errno codes, and the veneers of vector-traps: and pdriver-handler: (16.2.0-22): compile and link with nothing undefined. */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <complex.h>
#include <iconv.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <kernel.h>
#include "modg3.h"

_kernel_oserror *vta_handler (_kernel_swi_regs *r, void *pw, vectortrap_f trap, void *trappw)
{
  (void) pw;
  if (r->r[0] == 1) return trap (r, trappw);
  return r->r[0] == 2 ? (_kernel_oserror *) 1 : NULL;
}

_kernel_oserror *pd_info (int reason, _kernel_swi_regs *r, void *pw)
{
  (void) pw;
  r->r[0] = reason;
  return NULL;
}

static int go (void)
{
  struct stat st;
  struct stat64 st64;
  double complex z = csqrt (-4.0 + 0.0 * I);
  float complex w = cexpf (1.0f * I);
  char in[] = "abc", out[16], *ip = in, *op = out;
  size_t il = 3, ol = sizeof out;
  iconv_t cd = iconv_open ("UTF-16LE", "UTF-8");
  int fd = open ("a", O_RDWR | O_CREAT, 0644), d2;
  if (cd == (iconv_t) -1 || iconv (cd, &ip, &il, &op, &ol) == (size_t) -1 || iconv_close (cd)) return 1;
  if (fd < 3 || (d2 = dup (fd)) < 3 || dup2 (fd, d2) != d2 || fsync (fd) || fdatasync (fd)) return 2;
  if (stat ("a", &st) || lstat ("a", &st) || stat64 ("a", &st64) || lstat64 ("a", &st64) || access ("a", R_OK | W_OK) || access ("a", F_OK)) return 3;
  if (cimag (z) != 2.0 || crealf (w) > 1.0f || cabs (z) != 2.0) return 4;
  errno = EPROTO; if (strcmp (strerror (errno), "Protocol error")) return 5;
  errno = ENODATA; errno = ENOLINK; errno = EMULTIHOP; errno = ECANCELED; errno = ENOTRECOVERABLE; errno = EOWNERDEAD;
  { void *volatile p = (void *) pdentry; return p == 0; }
}

_kernel_oserror *modg3_cmd (const char *arg_string, int argc, int number, void *pw)
{
  (void) arg_string; (void) argc; (void) number; (void) pw;
  return go () ? NULL : NULL;
}
