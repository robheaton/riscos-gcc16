/* lstat (), lstat64 ()
 * Copyright (c) 2000-2009 UnixLib Developers
 */

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>

#include <internal/dev.h>
#include <internal/os.h>
#include <internal/local.h>
#include <unixlib/local.h>
#include <internal/swiparams.h>
#include <internal/unix.h>
#include <pthread.h>

/* #define DEBUG */
#ifdef DEBUG
#  include <sys/debug.h>
#endif

int
lstat (const char *filename, struct stat *buf)
{
  PTHREAD_UNSAFE

#ifdef DEBUG
  debug_printf ("lstat(file=%s)\n", filename);
#endif

  /* Perform a special check for devices.  */
  buf->st_dev = __getdevtype (filename, __get_riscosify_control());

  /* Perform the device specific open operation.  */
  return dev_funcall (buf->st_dev, lstat, (filename, buf));
}

int
lstat64 (const char *filename, struct stat64 *buf)
{
  /* struct stat64 is currently the same as struct stat.  */
  return lstat (filename, (struct stat *)buf);
}
