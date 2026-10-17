/* cwd.c - chdir, getcwd, rmdir (UnixLib 64; after libunixlib/unix/chdir.c, getcwd.c and rmdir.c of UnixLib, revised BSD licence, licenses/UnixLib-COPYING.txt).
   The current directory is that of the file system (OS_FSControl 0 sets it; "@" canonicalised gives it); getcwd gives it as a Unix path.  */
#include "ul64.h"
#include <unistd.h>

#define OS_FSControl 0x29

static int os_error_errno (void *e)				/* the FileSwitch error number in an error block, as an errno */
{
  switch (*(const unsigned *) e & 0xFF)
    {
    case 0xD6: return ENOENT;
    case 0xBD: case 0xC3: return EACCES;
    case 0xC2: return EBUSY;
    case 0xB4: return ENOTEMPTY;
    default: return EIO;
    }
}

/* a Unix name into a RISC OS name, without the suffix swapping (a directory is not a file) */
static char *dirname_ros (const char *name, char *buf, size_t size)
{
  char *e;
  int ctl = __get_riscosify_control ();
  __set_riscosify_control (ctl | __RISCOSIFY_NO_SUFFIX);
  e = __riscosify_std (name, 0, buf, size, NULL);
  __set_riscosify_control (ctl);
  if (!e) errno = ENAMETOOLONG;
  return e;
}

int chdir (const char *path)
{
  char b[MAXPATHLEN];
  long r[10];
  if (!path || !dirname_ros (path, b, sizeof b)) { if (!path) errno = EFAULT; return -1; }
  r[0] = 0; r[1] = (long) b;
  if (__os_swi (OS_FSControl, r)) { errno = os_error_errno ((void *) r[0]); return -1; }
  return 0;
}

char *getcwd (char *buf, size_t size)
{
  char ros[MAXPATHLEN], *out = buf;
  long r[10];
  if (buf && size == 0) { errno = EINVAL; return NULL; }
  r[0] = 37; r[1] = (long) "@"; r[2] = (long) ros; r[3] = 0; r[4] = 0; r[5] = sizeof ros;		/* OS_FSControl 37: canonicalise a path name */
  if (__os_swi (OS_FSControl, r)) { errno = os_error_errno ((void *) r[0]); return NULL; }
  if (!out) { out = (char *) malloc (size ? size : MAXPATHLEN); if (!out) { errno = ENOMEM; return NULL; } if (!size) size = MAXPATHLEN; }
  if (!__unixify_std (ros, out, size, __RISCOSIFY_FILETYPE_NOTSPECIFIED)) { if (!buf) free (out); errno = ERANGE; return NULL; }
  return out;
}

int rmdir (const char *path)
{
  char b[MAXPATHLEN];
  long r[10];
  if (!path || !dirname_ros (path, b, sizeof b)) { if (!path) errno = EFAULT; return -1; }
  r[0] = 17; r[1] = (long) b;
  if (__os_swi (OS_File, r)) { errno = EIO; return -1; }
  if (r[0] == 0) { errno = ENOENT; return -1; }
  if (r[0] == 1) { errno = ENOTDIR; return -1; }
  r[0] = 6; r[1] = (long) b;						/* OS_File 6 (delete) refuses a directory that is not empty */
  if (__os_swi (OS_File, r)) { errno = os_error_errno ((void *) r[0]); return -1; }
  return 0;
}
