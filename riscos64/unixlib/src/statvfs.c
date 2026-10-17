/* statvfs.c - statvfs for UnixLib 64: the free space and the size of the disc that holds the path (OS_FSControl 55, FreeSpace64: r0/r1 = the free space, r2/r3 = the size of the disc, as 64-bit values
   in two halves); the block size is 512.  Revised BSD licence (licenses/UnixLib-COPYING.txt) for the style of UnixLib; written for UnixLib 64.  */
#include "ul64.h"
#include <sys/statvfs.h>

int statvfs (const char *path, struct statvfs *buf)
{
  char b[MAXPATHLEN];
  long r[10];
  unsigned long long freespace, size;
  int ctl = __get_riscosify_control ();
  const char *ros;
  if (!path || !buf) { errno = EFAULT; return -1; }
  __set_riscosify_control (ctl | __RISCOSIFY_NO_SUFFIX);
  ros = __riscosify_std (path, 0, b, sizeof b, NULL) ? b : NULL;
  __set_riscosify_control (ctl);
  if (!ros) { errno = ENAMETOOLONG; return -1; }
  r[0] = 17; r[1] = (long) ros;
  if (__os_swi (OS_File, r) || r[0] == 0) { errno = ENOENT; return -1; }
  r[0] = 55; r[1] = (long) ros;
  if (__os_swi (0x29, r)) { errno = ENOSYS; return -1; }			/* OS_FSControl */
  freespace = ((unsigned long long) (r[1] & 0xFFFFFFFFUL) << 32) | (r[0] & 0xFFFFFFFFUL);
  size = ((unsigned long long) (r[3] & 0xFFFFFFFFUL) << 32) | (r[2] & 0xFFFFFFFFUL);
  memset (buf, 0, sizeof *buf);
  buf->f_bsize = buf->f_frsize = 512;
  buf->f_blocks = size / 512;
  buf->f_bfree = buf->f_bavail = freespace / 512;
  buf->f_namemax = 255;
  return 0;
}
