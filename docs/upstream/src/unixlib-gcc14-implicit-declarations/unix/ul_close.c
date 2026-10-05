/* __close (), close () for UnixLib
 * Copyright (c) 2000-2021 UnixLib Developers
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

#include <internal/unix.h>
#include <internal/os.h>
#include <internal/dev.h>
#include <internal/local.h>
#include <internal/fd.h>
#include <unixlib/local.h>
#ifdef __ARM_EABI__
#include <internal/swiparams.h>
#endif

/* #define DEBUG */
#ifdef DEBUG
#  include <sys/debug.h>
#endif

int
__close (struct __unixlib_fd *file_desc)
{
  if (__atomic_modify (&file_desc->devicehandle->refcount, -1) == 0)
    {
      /* We can close the file */
#ifdef DEBUG
      debug_printf (", closing\n");
#endif

      int x = dev_funcall (file_desc->devicehandle->type, close, (file_desc));

      struct __sul_process *sulproc = __ul_global.sulproc;
      sulproc->sul_free (sulproc->pid, file_desc->devicehandle);

      /* Invalidate this file descriptor.  */
      file_desc->devicehandle = NULL;

      if (x == -1)
        {
#ifdef DEBUG
          debug_printf ("         Failed!: %s\n",
			_kernel_last_oserror()->errmess);
#endif
          return -1;
        }
    }
  else
    {
      /* Invalidate this file descriptor.  */
      file_desc->devicehandle = NULL;
#ifdef DEBUG
      debug_printf (", duplicate\n");
#endif
    }

  return 0;
}

int
close (int fd)
{
  PTHREAD_UNSAFE_CANCELLATION

#ifdef DEBUG
  debug_printf ("-- close(fd=%d): ", fd);
#endif

#ifdef __ARM_EABI__
  if (IS_SHM_FD(fd))
    {
      _kernel_oserror *err;

      err = _swix(ARMEABISupport_ShmOp, _INR(0,1), ARMEABISUPPORT_SHMOP_CLOSE,
						   fd);
      return err ? __ul_seterr (err, __errno_from_armeabisupport (err->errnum)) : 0;
    }
#endif

  if (BADF (fd))
    {
#ifdef DEBUG
      debug_printf ("bad file descriptor\n");
#endif
      return __set_errno (EBADF);
    }

  struct __unixlib_fd *file_desc = getfd (fd);

  if (file_desc->devicehandle->type == DEV_SOCKET)
    FD_CLR (fd, &__socket_fd_set);

  return __close (file_desc);
}
