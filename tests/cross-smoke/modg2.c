/* modg2.c - the descriptor functions of the kit (open, read, write, lseek, fstat, ftruncate, isatty, ioctl, unlink, utime, readv, fdopen ...): compile with the headers of the kit and link with nothing undefined. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <kernel.h>
#include "modg2.h"

static int go (void)
{
  char buf[32];
  struct stat st;
  struct stat64 st64;
  struct utimbuf ub = { 0, 0 };
  struct timeval tv[2] = { { 0, 0 }, { 0, 0 } };
  struct iovec iv = { buf, sizeof buf };
  int n = 0, fd = open ("a", O_RDWR | O_CREAT | O_TRUNC, 0644);
  FILE *f;
  if (fd < 3 || creat ("b", 0644) < 0 || open64 ("c", O_RDONLY) < 0 || creat64 ("d", 0644) < 0) return 1;
  if (write (fd, "x", 1) != 1 || lseek (fd, 0, SEEK_SET) != 0 || lseek64 (fd, 0, SEEK_CUR) != 0 || read (fd, buf, sizeof buf) != 1) return 2;
  if (readv (fd, &iv, 1) < 0 || writev (fd, &iv, 1) < 0 || fstat (fd, &st) || fstat64 (fd, &st64) || !S_ISREG (st.st_mode)) return 3;
  if (ftruncate (fd, 0) || truncate ("a", 0) || isatty (fd) || ioctl (fd, FIONREAD, &n) || unlink ("a")) return 4;
  if (utime ("a", &ub) || utimes ("a", tv)) return 5;
  f = fdopen (fd, "r+");
  if (!f || fileno (f) != fd || close (fd)) return 6;
  return 0;
}
_kernel_oserror *modg2_cmd (const char *args, int argc, int number, void *pw)
{
  (void) args; (void) argc; (void) number; (void) pw;
  return go () ? (_kernel_oserror *) 0 : (_kernel_oserror *) 0;
}
