/* UnixLib stat()/stat64() implementation.
   Copyright (c) 2000-2008 UnixLib Developers.  */

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
stat (const char *filename, struct stat *buf)
{
  PTHREAD_UNSAFE

#ifdef DEBUG
  debug_printf ("stat(file=%s)\n", filename);
#endif

  /* Perform a special check for devices.  */
  buf->st_dev = __getdevtype (filename, __get_riscosify_control());

  /* Perform the device specific open operation.  */
  return dev_funcall (buf->st_dev, stat, (filename, buf));
}


int
stat64 (const char *filename, struct stat64 *buf)
{
  return stat(filename, (struct stat *)buf);
}

