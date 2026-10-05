/* recvfrom ()
 * Copyright (c) 1995 Sergio Monesi
 * Copyright (c) 1995-2011 UnixLib Developers
 */

#ifndef __TARGET_SCL__
#  include <pthread.h>
#else
#  include <errno.h>
#  include <stdio.h>
#endif
#include <sys/socket.h>

#ifndef __TARGET_SCL__
#  include <internal/unix.h>
#  include <internal/local.h>
#endif

ssize_t
recvfrom (int s, void *msg, size_t len, int flags,
	  struct sockaddr *from, socklen_t *fromlen)
{
#ifdef __TARGET_SCL__
  if ((unsigned)s < __FD_SOCKET_OFFSET)
    return __set_errno (EBADF);
  return _recvfrom (s - __FD_SOCKET_OFFSET, msg, len, flags, from, fromlen);
#else
  PTHREAD_UNSAFE_CANCELLATION

  if (__socket_valid (s) == -1)
    return -1;

  __touch_stack_buffer (msg, len);
  return _recvfrom ((int)getfd (s)->devicehandle->handle, msg, len, flags, from, fromlen);
#endif
}
