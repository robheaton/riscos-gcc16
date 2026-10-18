/* sockets.c - BSD sockets on the Internet module of RISC OS (the Socket_ SWIs, 0x41200 and up).  A socket is a descriptor, 300 + the module's socket number (above the file descriptors, which are the RISC OS handle + 3; FD_SETSIZE is 512 for it), that read, write, close, fcntl, ioctl, fstat,
   select and poll know about (they call the __ul64_sock_ functions here, through weak references so that programs without sockets do not link this file).

   What the Internet module wants and the C library gives differ in two ways, and this file converts: struct sockaddr starts with a length byte and a one byte family (4.4BSD), where the C library's starts with a
   two byte family; and the module's errors are BSD error numbers (EWOULDBLOCK is 35, ECONNREFUSED 61), where the C library's errno values are picolibc's.  The register block is 64 bits wide, so the pointers go
   in as they are.  Socket_Select works on sets of up to 32 sockets (one word of bits).  AF_INET only; AF_UNIX and AF_INET6 fail with EAFNOSUPPORT.

   Not tested on a real Internet module: the build service for Pyromaniac has the Internet SWIs disabled (the error "Internet SWIs are disabled" is what the calls return there, as ENETDOWN).  The SDK of RISC OS 64. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>

#define SOCKFD_BASE 300
#define NSOCK 64

#define Socket_Creat 0x41200
#define Socket_Bind 0x41201
#define Socket_Listen 0x41202
#define Socket_Accept 0x41203
#define Socket_Connect 0x41204
#define Socket_Recv 0x41205
#define Socket_Recvfrom 0x41206
#define Socket_Send 0x41208
#define Socket_Sendto 0x41209
#define Socket_Shutdown 0x4120B
#define Socket_Setsockopt 0x4120C
#define Socket_Getsockopt 0x4120D
#define Socket_Getpeername 0x4120E
#define Socket_Getsockname 0x4120F
#define Socket_Close 0x41210
#define Socket_Select 0x41211
#define Socket_Ioctl 0x41212
#define Socket_Read 0x41213
#define Socket_Write 0x41214

#define BSD_FIONBIO 0x8004667eUL		/* _IOW ('f', 126, int) */
#define BSD_FIONREAD 0x4004667fUL		/* _IOR ('f', 127, int) */

extern unsigned long swi_x (unsigned long swi, unsigned long regs[10]);
extern void __ul64_poll_signals (void);

static unsigned char nonblocking[NSOCK];
static unsigned char in_use[NSOCK];

/* BSD error number -> errno of the C library */
static int bsd_errno (unsigned num)
{
  switch (num)
    {
    case 1: return EPERM; case 2: return ENOENT; case 4: return EINTR; case 5: return EIO; case 9: return EBADF;
    case 12: return ENOMEM; case 13: return EACCES; case 14: return EFAULT; case 22: return EINVAL; case 24: return EMFILE;
    case 32: return EPIPE; case 35: return EWOULDBLOCK; case 36: return EINPROGRESS; case 37: return EALREADY; case 38: return ENOTSOCK;
    case 39: return EDESTADDRREQ; case 40: return EMSGSIZE; case 41: return EPROTOTYPE; case 42: return ENOPROTOOPT; case 43: return EPROTONOSUPPORT;
    case 45: return EOPNOTSUPP; case 47: return EAFNOSUPPORT; case 48: return EADDRINUSE; case 49: return EADDRNOTAVAIL; case 50: return ENETDOWN;
    case 51: return ENETUNREACH; case 52: return ENETRESET; case 53: return ECONNABORTED; case 54: return ECONNRESET; case 55: return ENOBUFS;
    case 56: return EISCONN; case 57: return ENOTCONN; case 60: return ETIMEDOUT; case 61: return ECONNREFUSED; case 62: return ELOOP;
    case 63: return ENAMETOOLONG; case 64: return EHOSTDOWN; case 65: return EHOSTUNREACH;
    default: return 0;
    }
}

/* a failed Socket_ SWI: r[0] is the error block (number, message) */
static int sock_fail (const unsigned long r[10])
{
  const unsigned *b = (const unsigned *) r[0];
  int e = 0;
  if (b)
    {
      unsigned num = b[0];
      e = bsd_errno (num);
      if (!e && (num & 0xFFFFFF00u)) e = bsd_errno (num & 0xFF);		/* a module number in the high bits: the low byte is the BSD error */
      if (!e && strstr ((const char *) (b + 1), "disabled")) e = ENETDOWN;
    }
  errno = e ? e : EIO;
  return -1;
}

static int is_sock (int fd) { return fd >= SOCKFD_BASE && fd < SOCKFD_BASE + NSOCK; }
int __ul64_is_sock (int fd) { return is_sock (fd) && in_use[fd - SOCKFD_BASE]; }
static int sock_of (int fd)
{
  if (!is_sock (fd) || !in_use[fd - SOCKFD_BASE]) { errno = EBADF; return -1; }
  return fd - SOCKFD_BASE;
}
static int new_fd (unsigned long s)
{
  if (s >= NSOCK) { unsigned long r[10] = { s }; swi_x (Socket_Close, r); errno = EMFILE; return -1; }
  in_use[s] = 1; nonblocking[s] = 0;
  return SOCKFD_BASE + (int) s;
}

/* the C library's sockaddr to the module's (length, family, then the same bytes), and back */
static int to_bsd (const struct sockaddr *a, socklen_t len, unsigned char *out, socklen_t *outlen)
{
  if (!a || len < 2 || len > 128) { errno = EINVAL; return -1; }
  if (a->sa_family != AF_INET) { errno = EAFNOSUPPORT; return -1; }
  if (len < 8) { errno = EINVAL; return -1; }
  memcpy (out, a, len);
  out[0] = (unsigned char) len;
  out[1] = AF_INET;
  *outlen = len;
  return 0;
}
static void from_bsd (const unsigned char *in, socklen_t inlen, struct sockaddr *a, socklen_t *alen)
{
  unsigned char tmp[128];
  socklen_t n = inlen > sizeof tmp ? (socklen_t) sizeof tmp : inlen;
  memcpy (tmp, in, n);
  if (n >= 2) { tmp[0] = in[1]; tmp[1] = 0; }				/* family: one byte to two bytes (little endian) */
  if (a && alen) { socklen_t k = *alen < n ? *alen : n; memcpy (a, tmp, k); *alen = n; }
}

int socket (int domain, int type, int protocol)
{
  unsigned long r[10];
  int fd, nb = type & SOCK_NONBLOCK;
  type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (domain != AF_INET) { errno = EAFNOSUPPORT; return -1; }
  r[0] = (unsigned long) domain; r[1] = (unsigned long) type; r[2] = (unsigned long) protocol;
  if (swi_x (Socket_Creat, r)) return sock_fail (r);
  fd = new_fd (r[0] & 0xFFFFFFFFUL);
  if (fd >= 0 && nb) { int one = 1; unsigned long q[10] = { r[0], BSD_FIONBIO, (unsigned long) &one }; if (!swi_x (Socket_Ioctl, q)) nonblocking[fd - SOCKFD_BASE] = 1; }
  return fd;
}
int socketpair (int d, int t, int p, int sv[2]) { (void) d; (void) t; (void) p; (void) sv; errno = EAFNOSUPPORT; return -1; }

int bind (int fd, const struct sockaddr *a, socklen_t len)
{
  unsigned char b[128]; socklen_t bl; int s = sock_of (fd);
  unsigned long r[10];
  if (s < 0 || to_bsd (a, len, b, &bl)) return -1;
  r[0] = (unsigned long) s; r[1] = (unsigned long) b; r[2] = bl;
  return swi_x (Socket_Bind, r) ? sock_fail (r) : 0;
}
int listen (int fd, int backlog)
{
  int s = sock_of (fd);
  unsigned long r[10];
  if (s < 0) return -1;
  r[0] = (unsigned long) s; r[1] = (unsigned long) backlog;
  return swi_x (Socket_Listen, r) ? sock_fail (r) : 0;
}
int accept (int fd, struct sockaddr *a, socklen_t *alen)
{
  unsigned char b[128];
  unsigned long len = sizeof b, r[10];
  int s = sock_of (fd), nfd;
  if (s < 0) return -1;
  for (;;)
    {
      r[0] = (unsigned long) s; r[1] = (unsigned long) b; r[2] = (unsigned long) &len; len = sizeof b;
      if (swi_x (Socket_Accept, r))
	{
	  if (bsd_errno (((unsigned *) r[0])[0]) == EWOULDBLOCK && !nonblocking[s]) { __ul64_poll_signals (); continue; }
	  return sock_fail (r);
	}
      break;
    }
  nfd = new_fd (r[0] & 0xFFFFFFFFUL);
  if (nfd >= 0) from_bsd (b, (socklen_t) len, a, alen);
  return nfd;
}
int connect (int fd, const struct sockaddr *a, socklen_t len)
{
  unsigned char b[128]; socklen_t bl; int s = sock_of (fd);
  unsigned long r[10];
  if (s < 0 || to_bsd (a, len, b, &bl)) return -1;
  r[0] = (unsigned long) s; r[1] = (unsigned long) b; r[2] = bl;
  return swi_x (Socket_Connect, r) ? sock_fail (r) : 0;
}

/* data: a blocking socket waits inside the module; Escape does not get through, so a blocking read polls: with the socket's own non-blocking mode the call returns EWOULDBLOCK, and the loop looks at signals */
static ssize_t xfer (unsigned long swi, int fd, void *buf, size_t n, int flags, const unsigned char *to, socklen_t tolen, unsigned char *from, unsigned long *fromlen)
{
  unsigned long r[10];
  int s = sock_of (fd);
  if (s < 0) return -1;
  for (;;)
    {
      memset (r, 0, sizeof r);
      r[0] = (unsigned long) s; r[1] = (unsigned long) buf; r[2] = n; r[3] = (unsigned long) flags;
      if (swi == Socket_Sendto) { r[4] = (unsigned long) to; r[5] = tolen; }
      if (swi == Socket_Recvfrom) { r[4] = (unsigned long) from; r[5] = (unsigned long) fromlen; }
      if (swi == Socket_Read || swi == Socket_Write) { r[3] = 0; }
      if (!swi_x (swi, r)) return (ssize_t) (r[0] & 0x7FFFFFFFFFFFFFFFUL);
      if (bsd_errno (((unsigned *) r[0])[0]) == EWOULDBLOCK && !nonblocking[s] && (swi == Socket_Recv || swi == Socket_Recvfrom || swi == Socket_Read)) { __ul64_poll_signals (); continue; }
      return sock_fail (r);
    }
}
ssize_t send (int fd, const void *b, size_t n, int f) { return xfer (Socket_Send, fd, (void *) b, n, f, 0, 0, 0, 0); }
ssize_t recv (int fd, void *b, size_t n, int f) { return xfer (Socket_Recv, fd, b, n, f, 0, 0, 0, 0); }
ssize_t sendto (int fd, const void *b, size_t n, int f, const struct sockaddr *a, socklen_t len)
{
  unsigned char t[128]; socklen_t tl;
  if (!a) return send (fd, b, n, f);
  if (to_bsd (a, len, t, &tl)) return -1;
  return xfer (Socket_Sendto, fd, (void *) b, n, f, t, tl, 0, 0);
}
ssize_t recvfrom (int fd, void *b, size_t n, int f, struct sockaddr *a, socklen_t *alen)
{
  unsigned char from[128];
  unsigned long fl = sizeof from;
  ssize_t r;
  if (!a) return recv (fd, b, n, f);
  r = xfer (Socket_Recvfrom, fd, b, n, f, 0, 0, from, &fl);
  if (r >= 0) from_bsd (from, (socklen_t) fl, a, alen);
  return r;
}
ssize_t __ul64_sock_read (int fd, void *b, size_t n) { return xfer (Socket_Read, fd, b, n, 0, 0, 0, 0, 0); }
ssize_t __ul64_sock_write (int fd, const void *b, size_t n) { return xfer (Socket_Write, fd, (void *) b, n, 0, 0, 0, 0, 0); }

int shutdown (int fd, int how)
{
  int s = sock_of (fd);
  unsigned long r[10];
  if (s < 0) return -1;
  r[0] = (unsigned long) s; r[1] = (unsigned long) how;
  return swi_x (Socket_Shutdown, r) ? sock_fail (r) : 0;
}
int __ul64_sock_close (int fd)
{
  int s = sock_of (fd);
  unsigned long r[10];
  if (s < 0) return -1;
  in_use[s] = 0;
  r[0] = (unsigned long) s;
  return swi_x (Socket_Close, r) ? sock_fail (r) : 0;
}

int setsockopt (int fd, int level, int name, const void *v, socklen_t len)
{
  int s = sock_of (fd);
  unsigned long r[10];
  if (s < 0) return -1;
  r[0] = (unsigned long) s; r[1] = (unsigned long) level; r[2] = (unsigned long) name; r[3] = (unsigned long) v; r[4] = len;
  return swi_x (Socket_Setsockopt, r) ? sock_fail (r) : 0;
}
int getsockopt (int fd, int level, int name, void *v, socklen_t *len)
{
  int s = sock_of (fd);
  unsigned long r[10];
  int n = len ? (int) *len : 0;
  if (s < 0) return -1;
  r[0] = (unsigned long) s; r[1] = (unsigned long) level; r[2] = (unsigned long) name; r[3] = (unsigned long) v; r[4] = (unsigned long) &n;
  if (swi_x (Socket_Getsockopt, r)) return sock_fail (r);
  if (len) *len = (socklen_t) n;
  return 0;
}
static int getname (unsigned long swi, int fd, struct sockaddr *a, socklen_t *alen)
{
  unsigned char b[128];
  unsigned long len = sizeof b, r[10];
  int s = sock_of (fd);
  if (s < 0) return -1;
  r[0] = (unsigned long) s; r[1] = (unsigned long) b; r[2] = (unsigned long) &len;
  if (swi_x (swi, r)) return sock_fail (r);
  from_bsd (b, (socklen_t) len, a, alen);
  return 0;
}
int getsockname (int fd, struct sockaddr *a, socklen_t *alen) { return getname (Socket_Getsockname, fd, a, alen); }
int getpeername (int fd, struct sockaddr *a, socklen_t *alen) { return getname (Socket_Getpeername, fd, a, alen); }

/* fcntl (F_GETFL / F_SETFL O_NONBLOCK) and ioctl (FIONBIO, FIONREAD) on a socket */
int __ul64_sock_nonblock (int fd, int on)
{
  int s = sock_of (fd), v = on != 0;
  unsigned long r[10];
  if (s < 0) return -1;
  r[0] = (unsigned long) s; r[1] = BSD_FIONBIO; r[2] = (unsigned long) &v;
  if (swi_x (Socket_Ioctl, r)) return sock_fail (r);
  nonblocking[s] = (unsigned char) v;
  return 0;
}
int __ul64_sock_getfl (int fd) { int s = sock_of (fd); return s < 0 ? -1 : O_RDWR | (nonblocking[s] ? O_NONBLOCK : 0); }
int __ul64_sock_ioctl (int fd, unsigned long request, void *arg)
{
  int s = sock_of (fd);
  unsigned long r[10];
  if (s < 0) return -1;
  if (request == FIONBIO) return __ul64_sock_nonblock (fd, arg ? *(int *) arg : 0);
  if (request != FIONREAD) { errno = ENOTTY; return -1; }
  r[0] = (unsigned long) s; r[1] = BSD_FIONREAD; r[2] = (unsigned long) arg;
  return swi_x (Socket_Ioctl, r) ? sock_fail (r) : 0;
}
int __ul64_sock_fstat (int fd, struct stat *st)
{
  if (sock_of (fd) < 0) return -1;
  memset (st, 0, sizeof *st);
  st->st_mode = S_IFSOCK | 0600;
  return 0;
}

/* are the sockets in the sets ready?  One look (a zero timeout); the callers loop.  sets: bit s of the word is socket s.  Returns the number of sockets ready, or -1. */
int __ul64_sock_ready (unsigned *rd, unsigned *wr, unsigned *ex, int nsock)
{
  unsigned long r[10];
  unsigned long tv[2] = { 0, 0 };				/* a zero timeout, whatever the width of the fields */
  r[0] = (unsigned long) nsock; r[1] = (unsigned long) rd; r[2] = (unsigned long) wr; r[3] = (unsigned long) ex; r[4] = (unsigned long) tv;
  if (swi_x (Socket_Select, r)) return sock_fail (r);
  return (int) (r[0] & 0x7FFFFFFF);
}
