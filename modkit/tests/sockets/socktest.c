/* Sockets in a module (sys/socket.h, netinet/in.h, arpa/inet.h, netdb.h of modkit): a server on the Internet module's SWIs, the error paths, the name functions, and the sockets as descriptors (close, read, write,
   readv, writev, ioctl, fcntl, fstat, dup, select: the descriptor functions work on a socket as they do in UnixLib). */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "kernel.h"
#include "swis.h"
#include "header.h"

_kernel_oserror *st_final (int fatal, int podule, void *pw)
{
  (void) fatal; (void) podule; (void) pw;
  return 0;
}

static _kernel_oserror *server (void)
{
  int s, c, on = 1, n, total = 0;
  struct sockaddr_in name, peer;
  socklen_t plen = sizeof peer;
  char buf[64];
  fd_set rd;
  struct timeval tv = { 0, 0 };
  s = socket (AF_INET, SOCK_STREAM, 0);
  printf ("socket: %d\n", s);
  if (setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) < 0) printf ("setsockopt failed %d\n", errno);
  memset (&name, 0, sizeof name);
  name.sin_len = sizeof name; name.sin_family = AF_INET; name.sin_port = htons (6000); name.sin_addr.s_addr = htonl (INADDR_ANY);
  printf ("bind: %d\n", bind (s, (struct sockaddr *) &name, sizeof name));
  printf ("listen: %d\n", listen (s, 5));
  printf ("ioctl: %d\n", socketioctl (s, FIONBIO, &on));
  FD_ZERO (&rd); FD_SET (s, &rd);
  printf ("select: %d, listener %s\n", select (s + 1, &rd, NULL, NULL, &tv), FD_ISSET (s, &rd) ? "ready" : "not ready");
  c = accept (s, (struct sockaddr *) &peer, &plen);
  printf ("accept: %s\n", c >= 0 ? "connected" : "failed");
  if (c < 0) { printf ("errno %d\n", errno); return 0; }
  while ((n = (int) recv (c, buf, sizeof buf, 0)) > 0)
    {
      int i;
      for (i = 0; i < n; i++) buf[i] = (char) toupper ((unsigned char) buf[i]);
      total += n;
      if (send (c, buf, (size_t) n, 0) != n) printf ("send failed\n");
    }
  printf ("recv: %d after %d bytes\n", n, total);
  if (n < 0) printf ("errno %d (%s)\n", errno, errno == EWOULDBLOCK ? "EWOULDBLOCK" : "other");
  printf ("close: %d %d\n", socketclose (c), socketclose (s));
  return 0;
}

static _kernel_oserror *errors (void)
{
  struct sockaddr_in name;
  int r, s;
  memset (&name, 0, sizeof name);
  errno = 0;
  r = bind (99, (struct sockaddr *) &name, sizeof name);          /* (99 is no descriptor: the table says EBADF, the Internet module is not asked) */
  printf ("bind(99): %d errno %d '%s'\n", r, errno, _inet_err ());
  errno = 0;
  r = (int) send (98, "x", 1, 0);
  printf ("send(98): %d errno %d\n", r, errno);
  errno = 0;
  r = socketclose (97);
  printf ("socketclose(97): %d errno %d\n", r, errno);
  s = socket (AF_INET, SOCK_STREAM, 0);
  errno = 0;
  r = accept (s, 0, 0);                                           /* (a socket that does not listen: the Internet module answers, and the message is kept) */
  printf ("accept: %d errno %d '%s'\n", r, errno, _inet_err ());
  close (s);
  return 0;
}

#define EN(e) ((e) == 0 ? "0" : (e) == EBADF ? "EBADF" : (e) == ENOTSOCK ? "ENOTSOCK" : (e) == ESPIPE ? "ESPIPE" : (e) == ENOTTY ? "ENOTTY" : (e) == ENOTSUP ? "ENOTSUP" : (e) == EMFILE ? "EMFILE" : (e) == EINVAL ? "EINVAL" : "other")

/* sockets as descriptors */
static _kernel_oserror *fds (void)
{
  int s, s2, c, d, on = 1, n, r, i, made = 0;
  struct sockaddr_in name, peer, sn;
  socklen_t plen = sizeof peer, slen = sizeof sn;
  struct stat st;
  char buf[64], b2[16], b3[16];
  struct iovec iv[2];
  fd_set rd, wr;
  struct timeval tv = { 0, 0 };
  static int many[80];

  s = socket (AF_INET, SOCK_STREAM, 0);
  printf ("descriptor: %s, the Internet module's number is %s\n", s >= 3 ? "3 or more" : "WRONG", __modlib_socket_handle (s) != s ? "another" : "the same");
  printf ("getfl: %d\n", fcntl (s, F_GETFL));
  r = ioctl (s, FIONBIO, &on); n = fcntl (s, F_GETFL);                  /* (one statement each: the order in which printf's arguments are worked out is not defined) */
  printf ("ioctl FIONBIO: %d, getfl: %d\n", r, n);
  r = fcntl (s, F_SETFL, 0); n = fcntl (s, F_GETFL);
  printf ("setfl 0: %d, getfl: %d\n", r, n);
  r = fcntl (s, F_SETFL, O_NONBLOCK); n = fcntl (s, F_GETFL);
  printf ("setfl O_NONBLOCK: %d, getfl: %d\n", r, n);
  r = fcntl (s, F_GETFD); n = fcntl (s, F_SETFD, 0);
  printf ("getfd: %d, setfd: %d\n", r, n);
  memset (&st, 0, sizeof st);
  printf ("fstat: %d, S_ISSOCK %d\n", fstat (s, &st), S_ISSOCK (st.st_mode) != 0);
  errno = 0; r = (int) lseek (s, 0, SEEK_SET);
  printf ("lseek: %d %s\n", r, EN (errno));
  errno = 0; r = isatty (s);
  printf ("isatty: %d %s\n", r, EN (errno));
  errno = 0;
  printf ("fdopen: %s %s\n", fdopen (s, "r+") ? "a FILE" : "NULL", EN (errno));
  /* dup: the Internet socket stays open until the last descriptor of it is closed */
  s2 = socket (AF_INET, SOCK_STREAM, 0);
  d = dup (s2);
  printf ("dup: %s\n", d > s2 ? "another descriptor" : "WRONG");
  r = close (s2); n = getsockname (d, (struct sockaddr *) &sn, &slen);
  printf ("close the original: %d, getsockname through the copy: %d\n", r, n);
  r = close (d);
  printf ("close the copy: %d\n", r);
  errno = 0; r = getsockname (d, (struct sockaddr *) &sn, &slen);
  printf ("getsockname after both are closed: %d %s\n", r, EN (errno));
  errno = 0; r = close (d);
  printf ("close again: %d %s\n", r, EN (errno));
  /* a server with the descriptor functions */
  memset (&name, 0, sizeof name);
  name.sin_len = sizeof name; name.sin_family = AF_INET; name.sin_port = htons (6000); name.sin_addr.s_addr = htonl (INADDR_ANY);
  r = bind (s, (struct sockaddr *) &name, sizeof name); n = listen (s, 5);
  printf ("bind: %d, listen: %d\n", r, n);
  FD_ZERO (&rd); FD_ZERO (&wr); FD_SET (s, &rd); FD_SET (1, &wr);
  n = select (s + 1, &rd, &wr, NULL, &tv);
  printf ("select: %d, the listener is %s, the screen (1) is %s for writing\n", n, FD_ISSET (s, &rd) ? "ready" : "not ready", FD_ISSET (1, &wr) ? "ready" : "not ready");
  c = accept (s, (struct sockaddr *) &peer, &plen);
  printf ("accept: %s, descriptor %s\n", c >= 0 ? "connected" : "failed", c > s ? "after the listener" : "WRONG");
  if (c < 0) return 0;
  memset (buf, 0, sizeof buf);
  n = (int) read (c, buf, 3);
  printf ("read: %d '%s'\n", n, buf);
  iv[0].iov_base = b2; iv[0].iov_len = 2; iv[1].iov_base = b3; iv[1].iov_len = 10;
  memset (b2, 0, sizeof b2); memset (b3, 0, sizeof b3);
  n = (int) readv (c, iv, 2);
  printf ("readv: %d '%s' '%s'\n", n, b2, b3);
  printf ("read at the end: %d\n", (int) read (c, buf, sizeof buf));
  printf ("write: %d\n", (int) write (c, "XYZ", 3));
  iv[0].iov_base = "AB"; iv[0].iov_len = 2; iv[1].iov_base = "CDE"; iv[1].iov_len = 3;
  printf ("writev: %d\n", (int) writev (c, iv, 2));
  r = close (c); n = close (s);
  printf ("close: %d, %d\n", r, n);
  /* the errors of the descriptor layer */
  errno = 0; r = socketclose (1);
  printf ("socketclose(1): %d %s\n", r, EN (errno));
  errno = 0; r = bind (1, (struct sockaddr *) &name, sizeof name);
  printf ("bind(1): %d %s\n", r, EN (errno));
  errno = 0; r = (int) read (97, buf, 1);
  printf ("read(97): %d %s\n", r, EN (errno));
  errno = 0; r = ioctl (97, FIONBIO, &on);
  printf ("ioctl(97): %d %s\n", r, EN (errno));
  FD_ZERO (&rd); FD_SET (97, &rd);
  errno = 0; r = select (98, &rd, NULL, NULL, &tv);
  printf ("select(97): %d %s\n", r, EN (errno));
  errno = 0; r = select (FD_SETSIZE + 1, NULL, NULL, NULL, &tv);
  printf ("select(too many): %d %s\n", r, EN (errno));
  /* the table: how many sockets can be open at once, and nothing is left open when it is full */
  for (i = 0; i < 80; i++)
    {
      many[i] = socket (AF_INET, SOCK_STREAM, 0);
      if (many[i] < 0) break;
      made++;
    }
  printf ("sockets made: %d, then %s\n", made, EN (errno));
  for (i = 0; i < made; i++) close (many[i]);
  printf ("all closed\n");
  return 0;
}

static _kernel_oserror *names (void)
{
  struct in_addr a;
  struct hostent *h;
  struct servent *sv;
  struct protoent *pr;
  char buf[32];
  printf ("inet_addr(10.1.2.3) = %08x\n", inet_addr ("10.1.2.3"));
  printf ("inet_addr(127.1) = %08x, inet_addr(0x7f000001) = %08x, inet_addr(300.1.1.1) = %08x\n", inet_addr ("127.1"), inet_addr ("0x7f000001"), inet_addr ("300.1.1.1"));
  a.s_addr = htonl (0xC6336464);
  printf ("inet_ntoa = %s, inet_aton(1.2.3.4.5) = %d\n", inet_ntoa (a), inet_aton ("1.2.3.4.5", &a));
  printf ("inet_pton = %d, inet_ntop = %s\n", inet_pton (AF_INET, "8.8.4.4", &a), inet_ntop (AF_INET, &a, buf, sizeof buf));
  printf ("htons(0x1234) = %04x, htonl(0x12345678) = %08x\n", htons (0x1234), (unsigned) htonl (0x12345678));
  h = gethostbyname ("10.20.30.40");
  printf ("gethostbyname(10.20.30.40): %s, %s\n", h ? h->h_name : "NULL", h ? inet_ntoa (*(struct in_addr *) h->h_addr_list[0]) : "-");
  h = gethostbyname ("example.test");
  printf ("gethostbyname(example.test): %s, %s, %d addresses\n", h ? h->h_name : "NULL", h ? inet_ntoa (*(struct in_addr *) h->h_addr_list[0]) : "-",
          h ? (h->h_addr_list[1] ? 2 : 1) : 0);
  h = gethostbyname ("nowhere.test");
  printf ("gethostbyname(nowhere.test): %s, h_errno %d (%s)\n", h ? "found" : "NULL", h_errno, hstrerror (h_errno));
  sv = getservbyname ("http", "tcp");
  printf ("getservbyname(http): %d %s\n", sv ? ntohs ((unsigned short) sv->s_port) : -1, sv ? sv->s_proto : "-");
  sv = getservbyport ((int) htons (514), "udp");
  printf ("getservbyport(514): %s\n", sv ? sv->s_name : "NULL");
  pr = getprotobyname ("udp");
  printf ("getprotobyname(udp): %d, getprotobynumber(6): %s\n", pr ? pr->p_proto : -1, getprotobynumber (6) ? getprotobynumber (6)->p_name : "NULL");
  return 0;
}

/* a socket made with the raw SWI Socket_Creat, turned into a descriptor, and closed with close (): no socket (), accept () or other call of the socket code before it (so that the descriptor layer must find the
   Internet module's side of close () by itself) */
static _kernel_oserror *raw (void)
{
  int h = -1, fd, r;
  _kernel_oserror *e = _swix (0x61200, _INR (0, 2) | _OUT (0), AF_INET, SOCK_STREAM, 0, &h);
  fd = e ? -1 : __modlib_socket_fd (h);
  printf ("raw: socket %s, descriptor %s, handle back %s\n", h >= 0 ? "ok" : "bad", fd >= 3 ? "ok" : "bad", fd >= 3 && __modlib_socket_handle (fd) == h ? "ok" : "bad");
  r = fd >= 0 ? close (fd) : -2;
  printf ("raw: close %d\n", r);
  errno = 0;
  r = fd >= 0 ? close (fd) : -2;
  printf ("raw: close again %d %s\n", r, errno == EBADF ? "EBADF" : "?");
  return 0;
}

/* the end of a module or program: __modlib_closeall () (and exit ()) close the sockets too, once socket () has been used */
static _kernel_oserror *endt (void)
{
  int a = socket (AF_INET, SOCK_STREAM, 0), b = socket (AF_INET, SOCK_DGRAM, 0), c = dup (a), n;
  printf ("end: three descriptors %s\n", a >= 3 && b >= 3 && c >= 3 ? "ok" : "bad");
  n = __modlib_closeall ();
  printf ("end: __modlib_closeall closed %d\n", n);
  errno = 0;
  printf ("end: close again %d %s\n", close (a), errno == EBADF ? "EBADF" : "?");
  return 0;
}

_kernel_oserror *st_command (const char *arg_string, int argc, int number, void *pw)
{
  (void) arg_string; (void) argc; (void) pw;
  switch (number)
    {
    case CMD_SockTest_Server: return server ();
    case CMD_SockTest_Errors: return errors ();
    case CMD_SockTest_Fds: return fds ();
    case CMD_SockTest_Raw: return raw ();
    case CMD_SockTest_End: return endt ();
    default: return names ();
    }
}
