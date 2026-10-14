/* sockets.c - the BSD socket calls of sys/socket.h on the Internet module's SWIs (Socket_Creat ... Socket_Version: XSocket_* = &61200 ...), as the socklib of the RISC OS sources does:
   the arguments go into R0 - R5 in the order of the C call, the result is R0; an error gives -1, with errno set from the error number (the Internet module's errors are &20E00 + the UNIX number;
   a number above ENEEDAUTH becomes ESRCH, any other error number is errno as it is) and the block kept in _inet_error.
   A socket is a DESCRIPTOR of the table of open () (fdio.c, kind K_SOCKET; the number of the Internet module's own socket is inside): socket () and accept () return one, every call here takes one and turns it
   into the module's number, and close, read, write, readv, writev, ioctl, fcntl, fstat and dup of fdio.c work on it (they reach this file through __modlib_sock_op, which is set when the first socket is made, so that a
   module without sockets does not take this file with close ()).  The names socketclose, socketread ... of the Shared C Library / TCPIPLibs are the same calls.  select () takes descriptors too. */
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include "kernel.h"
#include "fileimpl.h"

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
#define HANDLE(s) int h = __modlib_socket_handle (s); if (h < 0) return -1          /* H: the number of the Internet module's socket of descriptor S, or the call fails (EBADF / ENOTSOCK) */

/* what close, read, write, readv, writev and ioctl of fdio.c do to a socket */
static int sock_op (int op, int h, void *buf, unsigned long n, void *arg)
{
  switch (op)
    {
    case SOP_CLOSE: return OK (sock (0x10, U (h), 0, 0, 0, 0, 0));
    case SOP_READ: return sock (0x13, U (h), U (buf), U (n), 0, 0, 0);
    case SOP_WRITE: return sock (0x14, U (h), U (buf), U (n), 0, 0, 0);
    case SOP_READV: return sock (0x16, U (h), U (buf), U (n), 0, 0, 0);
    case SOP_WRITEV: return sock (0x17, U (h), U (buf), U (n), 0, 0, 0);
    case SOP_IOCTL: return OK (sock (0x12, U (h), U (n), U (arg), 0, 0, 0));
    }
  errno = EINVAL;
  return -1;
}

/* a socket H of the Internet module becomes a descriptor: for code that gets its sockets from the Socket_ SWIs itself (this file, so that close (), read () ... on the descriptor can reach the Internet module even
   when socket () and accept () are never called) */
int __modlib_socket_fd (int handle)
{
  __modlib_sock_op = sock_op;
  __modlib_sock_end_hook = __modlib_sockets_closeall;             /* and exit () and __modlib_closeall () close the sockets */
  __modlib_register_end ();
  return __modlib_socket_new (handle);
}

/* a new socket H of the Internet module becomes a descriptor (when the table is full the socket is closed again) */
static int wrap (int h)
{
  int fd = __modlib_socket_fd (h);
  if (fd < 0)
    {
      int e = errno;
      sock (0x10, U (h), 0, 0, 0, 0, 0);
      errno = e;
    }
  return fd;
}

int socket (int domain, int type, int protocol)
{
  int h = sock (0x00, U (domain), U (type), U (protocol), 0, 0, 0);
  return h < 0 ? -1 : wrap (h);
}
int bind (int s, const struct sockaddr *addr, socklen_t len) { HANDLE (s); return OK (sock (0x01, U (h), U (addr), len, 0, 0, 0)); }
int listen (int s, int backlog) { HANDLE (s); return OK (sock (0x02, U (h), U (backlog), 0, 0, 0, 0)); }
int connect (int s, const struct sockaddr *addr, socklen_t len) { HANDLE (s); return OK (sock (0x04, U (h), U (addr), len, 0, 0, 0)); }
ssize_t recv (int s, void *buf, size_t len, int flags) { HANDLE (s); return sock (0x05, U (h), U (buf), U (len), U (flags), 0, 0); }
ssize_t send (int s, const void *buf, size_t len, int flags) { HANDLE (s); return sock (0x08, U (h), U (buf), U (len), U (flags), 0, 0); }
ssize_t sendto (int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
  HANDLE (s);
  return sock (0x09, U (h), U (buf), U (len), U (flags), U (to), tolen);
}
int shutdown (int s, int how) { HANDLE (s); return OK (sock (0x0B, U (h), U (how), 0, 0, 0, 0)); }
int setsockopt (int s, int level, int name, const void *value, socklen_t len)
{
  HANDLE (s);
  return OK (sock (0x0C, U (h), U (level), U (name), U (value), len, 0));
}
int getsockopt (int s, int level, int name, void *value, socklen_t *len)
{
  HANDLE (s);
  return OK (sock (0x0D, U (h), U (level), U (name), U (value), U (len), 0));
}
int socketclose (int s)
{
  if (__modlib_socket_handle (s) < 0) return -1;
  return close (s);                                              /* (the Internet module's socket goes with the last descriptor: dup () makes more) */
}
/* select () on descriptors: the sockets among them are turned into the Internet module's numbers (its sets are indexed by those), asked, and turned back; a file, the screen and the keyboard are always ready to
   read and write (never for exceptions), so when one is in a set the sockets are only looked at (a zero time) */
int select (int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
  static const struct timeval zero = { 0, 0 };
  fd_set sets[3], old;
  fd_set *in[3];
  int fd, k, maxh = -1, nfiles = 0, count = 0;
  in[0] = readfds; in[1] = writefds; in[2] = exceptfds;
  if (nfds < 0 || nfds > FD_SETSIZE) { errno = EINVAL; return -1; }
  memset (sets, 0, sizeof sets);
  for (fd = 0; fd < nfds; fd++)
    for (k = 0; k < 3; k++)
      if (in[k] && FD_ISSET (fd, in[k]))
	{
	  int h, kind = __modlib_fd_info (fd, &h);
	  if (kind == 0) return -1;                                  /* EBADF */
	  if (kind == 1)
	    {
	      if (h < 0 || h >= FD_SETSIZE) { errno = EINVAL; return -1; }
	      FD_SET (h, &sets[k]);
	      if (h > maxh) maxh = h;
	    }
	  else if (k < 2) nfiles++;
	}
  if (maxh >= 0 || !nfiles)                                      /* (no descriptors at all: the wait that select (0, 0, 0, 0, &time) is) */
    if (sock (0x11, U (maxh + 1), U (readfds ? &sets[0] : 0), U (writefds ? &sets[1] : 0), U (exceptfds ? &sets[2] : 0), U (nfiles ? &zero : timeout), 0) < 0) return -1;
  for (k = 0; k < 3; k++)
    if (in[k])
      {
	old = *in[k];
	memset (in[k], 0, sizeof *in[k]);
	for (fd = 0; fd < nfds; fd++)
	  if (FD_ISSET (fd, &old))
	    {
	      int h, kind = __modlib_fd_info (fd, &h);
	      if (kind == 1 ? FD_ISSET (h, &sets[k]) : (kind == 2 && k < 2)) { FD_SET (fd, in[k]); count++; }
	    }
      }
  return count;
}
int socketioctl (int s, unsigned long cmd, void *arg)
{
  if (__modlib_socket_handle (s) < 0) return -1;
  return ioctl (s, cmd, arg);                                    /* (ioctl of fdio.c keeps the non-blocking flag of the descriptor right) */
}
ssize_t socketread (int s, void *buf, size_t len) { HANDLE (s); return sock (0x13, U (h), U (buf), U (len), 0, 0, 0); }
ssize_t socketwrite (int s, const void *buf, size_t len) { HANDLE (s); return sock (0x14, U (h), U (buf), U (len), 0, 0, 0); }
int socketstat (int s, void *stat) { HANDLE (s); return OK (sock (0x15, U (h), U (stat), 0, 0, 0, 0)); }
ssize_t socketreadv (int s, const struct iovec *iov, int iovcnt) { HANDLE (s); return sock (0x16, U (h), U (iov), U (iovcnt), 0, 0, 0); }
ssize_t socketwritev (int s, const struct iovec *iov, int iovcnt) { HANDLE (s); return sock (0x17, U (h), U (iov), U (iovcnt), 0, 0, 0); }
int getstablesize (void) { return sock (0x18, 0, 0, 0, 0, 0, 0); }
/* the variants with the BSD 4.4 sockaddr (a length byte): Socket_Accept_1 ... */
int accept (int s, struct sockaddr *addr, socklen_t *len)
{
  int n;
  HANDLE (s);
  n = sock (0x1B, U (h), U (addr), U (len), 0, 0, 0);
  return n < 0 ? -1 : wrap (n);
}
ssize_t recvfrom (int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
  HANDLE (s);
  return sock (0x1C, U (h), U (buf), U (len), U (flags), U (from), U (fromlen));
}
ssize_t recvmsg (int s, struct msghdr *msg, int flags) { HANDLE (s); return sock (0x1D, U (h), U (msg), U (flags), 0, 0, 0); }
ssize_t sendmsg (int s, const struct msghdr *msg, int flags) { HANDLE (s); return sock (0x1E, U (h), U (msg), U (flags), 0, 0, 0); }
int getpeername (int s, struct sockaddr *addr, socklen_t *len) { HANDLE (s); return OK (sock (0x1F, U (h), U (addr), U (len), 0, 0, 0)); }
int getsockname (int s, struct sockaddr *addr, socklen_t *len) { HANDLE (s); return OK (sock (0x20, U (h), U (addr), U (len), 0, 0, 0)); }
