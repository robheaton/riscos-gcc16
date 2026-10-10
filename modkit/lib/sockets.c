/* sockets.c - the BSD socket calls of sys/socket.h on the Internet module's SWIs (Socket_Creat ... Socket_Version: XSocket_* = &61200 ...), as the socklib of the RISC OS sources does:
   the arguments go into R0 - R5 in the order of the C call, the result is R0; an error gives -1, with errno set from the error number (the Internet module's errors are &20E00 + the UNIX number;
   a number above ENEEDAUTH becomes ESRCH, any other error number is errno as it is) and the block kept in _inet_error. */
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include "kernel.h"

extern _kernel_oserror *__modlib_xswi (unsigned swi_x, unsigned *regs);

_kernel_oserror _inet_error;

char *_inet_err (void)
{
  return _inet_error.errmess;
}

#define XSOCKET 0x61200u
#define ENEEDAUTH_ 81

static int sock_fail (const _kernel_oserror *e)
{
  unsigned n = (unsigned) e->errnum;
  if (n - 0x20C00u >= 0x200u && n - 0x20C00u < 0x280u)         /* &20E00 - &20E7F: a UNIX error number */
    {
      n &= 0xFF;
      if (n > ENEEDAUTH_) n = ESRCH;
    }
  errno = (int) n;
  memcpy (&_inet_error, e, sizeof _inet_error);
  return -1;
}

/* the SWI XSocket_* + OFFSET with up to six arguments: R0, or -1 with errno set */
static int sock (unsigned offset, unsigned a0, unsigned a1, unsigned a2, unsigned a3, unsigned a4, unsigned a5)
{
  unsigned r[10] = { a0, a1, a2, a3, a4, a5, 0, 0, 0, 0 };
  _kernel_oserror *e = __modlib_xswi (XSOCKET + offset, r);
  if (e) return sock_fail (e);
  return (int) r[0];
}

#define U(x) ((unsigned) (x))
#define OK(x) ((x) < 0 ? -1 : 0)                                 /* the calls that have no result */

int socket (int domain, int type, int protocol) { return sock (0x00, U (domain), U (type), U (protocol), 0, 0, 0); }
int bind (int s, const struct sockaddr *addr, socklen_t len) { return OK (sock (0x01, U (s), U (addr), len, 0, 0, 0)); }
int listen (int s, int backlog) { return OK (sock (0x02, U (s), U (backlog), 0, 0, 0, 0)); }
int connect (int s, const struct sockaddr *addr, socklen_t len) { return OK (sock (0x04, U (s), U (addr), len, 0, 0, 0)); }
ssize_t recv (int s, void *buf, size_t len, int flags) { return sock (0x05, U (s), U (buf), U (len), U (flags), 0, 0); }
ssize_t send (int s, const void *buf, size_t len, int flags) { return sock (0x08, U (s), U (buf), U (len), U (flags), 0, 0); }
ssize_t sendto (int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
  return sock (0x09, U (s), U (buf), U (len), U (flags), U (to), tolen);
}
int shutdown (int s, int how) { return OK (sock (0x0B, U (s), U (how), 0, 0, 0, 0)); }
int setsockopt (int s, int level, int name, const void *value, socklen_t len)
{
  return OK (sock (0x0C, U (s), U (level), U (name), U (value), len, 0));
}
int getsockopt (int s, int level, int name, void *value, socklen_t *len)
{
  return OK (sock (0x0D, U (s), U (level), U (name), U (value), U (len), 0));
}
int socketclose (int s) { return OK (sock (0x10, U (s), 0, 0, 0, 0, 0)); }
int select (int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  return sock (0x11, U (nfds), U (readfds), U (writefds), U (exceptfds), U (timeout), 0);
}
int socketioctl (int s, unsigned long cmd, void *arg) { return OK (sock (0x12, U (s), U (cmd), U (arg), 0, 0, 0)); }
ssize_t socketread (int s, void *buf, size_t len) { return sock (0x13, U (s), U (buf), U (len), 0, 0, 0); }
ssize_t socketwrite (int s, const void *buf, size_t len) { return sock (0x14, U (s), U (buf), U (len), 0, 0, 0); }
int socketstat (int s, void *stat) { return OK (sock (0x15, U (s), U (stat), 0, 0, 0, 0)); }
ssize_t socketreadv (int s, const struct iovec *iov, int iovcnt) { return sock (0x16, U (s), U (iov), U (iovcnt), 0, 0, 0); }
ssize_t socketwritev (int s, const struct iovec *iov, int iovcnt) { return sock (0x17, U (s), U (iov), U (iovcnt), 0, 0, 0); }
int getstablesize (void) { return sock (0x18, 0, 0, 0, 0, 0, 0); }
/* the variants with the BSD 4.4 sockaddr (a length byte): Socket_Accept_1 ... */
int accept (int s, struct sockaddr *addr, socklen_t *len) { return sock (0x1B, U (s), U (addr), U (len), 0, 0, 0); }
ssize_t recvfrom (int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
  return sock (0x1C, U (s), U (buf), U (len), U (flags), U (from), U (fromlen));
}
ssize_t recvmsg (int s, struct msghdr *msg, int flags) { return sock (0x1D, U (s), U (msg), U (flags), 0, 0, 0); }
ssize_t sendmsg (int s, const struct msghdr *msg, int flags) { return sock (0x1E, U (s), U (msg), U (flags), 0, 0, 0); }
int getpeername (int s, struct sockaddr *addr, socklen_t *len) { return OK (sock (0x1F, U (s), U (addr), U (len), 0, 0, 0)); }
int getsockname (int s, struct sockaddr *addr, socklen_t *len) { return OK (sock (0x20, U (s), U (addr), U (len), 0, 0, 0)); }
