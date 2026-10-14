/* sockhw.c - the module SockHw of pack/module45: the socket calls of modkit (sys/socket.h ...) on the real Internet module, and the OSLib veneers of libOSLib32.a (X and non-X) on the real kernel.
     *SockHw_Test    everything on the loopback address: TCP (connect, accept, send, recv, socketwrite, socketread, shutdown), UDP (sendto, recvfrom), errors (EBADF, EWOULDBLOCK, ECONNREFUSED), the name
                     functions and the OSLib calls
     *SockHw_Raise   a non-X OSLib function that fails: the error ends the command (OS_GenerateError)
     *SockHw_Try     runs *SockHw_Raise through OS_CLI and prints the error that comes back (an Obey file stops at an error)
     *SockHw_Fds     sockets as descriptors (16.2.0-23): fcntl, ioctl, read, write, readv, writev, dup, select with a descriptor that is not a socket, fstat, the full table, a socket made with the raw SWI; and the
                     interface requests (SIOCGIFCONF, SIOCGIFADDR, SIOCGIFFLAGS, SIOCGIFNETMASK of lo0)
     *SockHw_Raw     a socket made with the raw SWI and turned into a descriptor, closed by close () with no socket () call before it (run it first after RMLoad)
     *SockHw_Serve   an echo server on port 6000 for a client on another machine (tests/hw-sockets.py) */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <kernel.h>
#include <swis.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "oslib/os.h"
#include "header.h"

static int checks, fails;
static void check (int ok, const char *what)
{
  checks++;
  if (!ok) fails++;
  printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
}

_kernel_oserror *sh_final (int fatal, int podule, void *pw) { (void) fatal; (void) podule; (void) pw; return 0; }

static struct sockaddr_in addr_of (const char *ip, int port)
{
  struct sockaddr_in a;
  memset (&a, 0, sizeof a);
  a.sin_len = sizeof a; a.sin_family = AF_INET; a.sin_port = htons ((unsigned short) port); a.sin_addr.s_addr = inet_addr (ip);
  return a;
}

/* wait up to SECS seconds for socket S to be readable (or writable); the number of sockets that are ready */
static int wait_for (int s, int writing, int secs)
{
  fd_set set;
  struct timeval tv;
  FD_ZERO (&set); FD_SET (s, &set);
  tv.tv_sec = secs; tv.tv_usec = 0;
  return writing ? select (s + 1, NULL, &set, NULL, &tv) : select (s + 1, &set, NULL, NULL, &tv);
}

static int nonblocking (int s) { int on = 1; return socketioctl (s, FIONBIO, &on); }

static void tcp (void)
{
  struct sockaddr_in a, peer;
  socklen_t plen = sizeof peer;
  int l, c, k, on = 1, v, r, e;
  socklen_t vl = sizeof v;
  char buf[32];
  printf ("TCP on 127.0.0.1\n");
  l = socket (AF_INET, SOCK_STREAM, 0);
  check (l >= 0, "socket (AF_INET, SOCK_STREAM, 0)");
  check (setsockopt (l, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) == 0, "setsockopt SO_REUSEADDR");
  v = -1; vl = sizeof v;
  check (getsockopt (l, SOL_SOCKET, SO_TYPE, &v, &vl) == 0 && v == SOCK_STREAM, "getsockopt SO_TYPE is SOCK_STREAM");
  a = addr_of ("0.0.0.0", 6000);
  check (bind (l, (struct sockaddr *) &a, sizeof a) == 0, "bind to port 6000");
  check (listen (l, 5) == 0, "listen");
  check (nonblocking (l) == 0, "socketioctl FIONBIO");
  plen = sizeof peer; memset (&peer, 0, sizeof peer);
  check (getsockname (l, (struct sockaddr *) &peer, &plen) == 0 && peer.sin_family == AF_INET && ntohs (peer.sin_port) == 6000, "getsockname: AF_INET, port 6000");
  printf ("  INFO  getsockname: length %u, sin_len %u\n", plen, peer.sin_len);
  c = socket (AF_INET, SOCK_STREAM, 0);
  check (c >= 0 && nonblocking (c) == 0, "second socket, non-blocking");
  a = addr_of ("127.0.0.1", 6000);
  r = connect (c, (struct sockaddr *) &a, sizeof a); e = errno;
  check (r == 0 || (r < 0 && e == EINPROGRESS), "connect (non-blocking): 0 or EINPROGRESS");
  printf ("  INFO  connect gave %d errno %d\n", r, e);
  check (wait_for (l, 0, 3) == 1, "select: the listening socket is readable");
  plen = sizeof peer; memset (&peer, 0, sizeof peer);
  k = accept (l, (struct sockaddr *) &peer, &plen);
  check (k >= 0, "accept");
  check (peer.sin_family == AF_INET && peer.sin_addr.s_addr == inet_addr ("127.0.0.1") && peer.sin_port != 0, "accept: the peer is 127.0.0.1 with a port");
  printf ("  INFO  peer %s:%u length %u sin_len %u\n", inet_ntoa (peer.sin_addr), ntohs (peer.sin_port), plen, peer.sin_len);
  check (wait_for (c, 1, 3) == 1, "select: the connecting socket is writable");
  v = -1; vl = sizeof v;
  check (getsockopt (c, SOL_SOCKET, SO_ERROR, &v, &vl) == 0 && v == 0, "getsockopt SO_ERROR is 0");
  nonblocking (k);
  check (send (c, "ping", 4, 0) == 4, "send \"ping\"");
  check (wait_for (k, 0, 3) == 1, "select: the accepted socket is readable");
  memset (buf, 0, sizeof buf);
  check (recv (k, buf, sizeof buf, 0) == 4 && !memcmp (buf, "ping", 4), "recv: \"ping\"");
  check (socketwrite (k, "PONG", 4) == 4, "socketwrite \"PONG\"");
  check (wait_for (c, 0, 3) == 1, "select: the first socket is readable");
  memset (buf, 0, sizeof buf);
  check (socketread (c, buf, sizeof buf) == 4 && !memcmp (buf, "PONG", 4), "socketread: \"PONG\"");
  errno = 0;
  r = (int) recv (c, buf, sizeof buf, 0);
  check (r == -1 && errno == EWOULDBLOCK, "recv with nothing to read (non-blocking): -1 and EWOULDBLOCK");
  printf ("  INFO  errno %d, _inet_err: \"%s\"\n", errno, _inet_err ());
  v = -1;
  check (socketioctl (k, FIONREAD, &v) == 0 && v == 0, "socketioctl FIONREAD: 0 bytes waiting");
  check (shutdown (k, SHUT_WR) == 0, "shutdown (SHUT_WR)");
  check (wait_for (c, 0, 3) == 1 && recv (c, buf, sizeof buf, 0) == 0, "the other side reads end of file (0)");
  check (socketclose (k) == 0 && socketclose (c) == 0 && socketclose (l) == 0, "socketclose of the three");
  errno = 0;
  check (socketclose (k) == -1 && errno == EBADF, "socketclose of a closed socket: -1 and EBADF");
  printf ("  INFO  errno %d\n", errno);
}

static void udp (void)
{
  struct sockaddr_in a, from;
  socklen_t fl = sizeof from;
  int s1, s2;
  char buf[32];
  printf ("UDP on 127.0.0.1\n");
  s1 = socket (AF_INET, SOCK_DGRAM, 0); s2 = socket (AF_INET, SOCK_DGRAM, 0);
  check (s1 >= 0 && s2 >= 0, "two datagram sockets");
  a = addr_of ("0.0.0.0", 6001);
  check (bind (s1, (struct sockaddr *) &a, sizeof a) == 0, "bind to port 6001");
  nonblocking (s1);
  a = addr_of ("127.0.0.1", 6001);
  check (sendto (s2, "dgram", 5, 0, (struct sockaddr *) &a, sizeof a) == 5, "sendto 127.0.0.1:6001");
  check (wait_for (s1, 0, 3) == 1, "select: the datagram socket is readable");
  memset (buf, 0, sizeof buf); memset (&from, 0, sizeof from);
  check (recvfrom (s1, buf, sizeof buf, 0, (struct sockaddr *) &from, &fl) == 5 && !memcmp (buf, "dgram", 5), "recvfrom: \"dgram\"");
  check (from.sin_family == AF_INET && from.sin_addr.s_addr == inet_addr ("127.0.0.1"), "recvfrom: from 127.0.0.1");
  printf ("  INFO  from %s:%u length %u\n", inet_ntoa (from.sin_addr), ntohs (from.sin_port), fl);
  check (socketclose (s1) == 0 && socketclose (s2) == 0, "socketclose of the two");
}

#define EN(e) ((e) == 0 ? "0" : (e) == EBADF ? "EBADF" : (e) == ENOTSOCK ? "ENOTSOCK" : (e) == EMFILE ? "EMFILE" : (e) == ENFILE ? "ENFILE" : "other")

/* sockets as descriptors, on the real Internet module (16.2.0-23) */
static void fds (void)
{
  struct sockaddr_in a, peer;
  socklen_t pl = sizeof peer;
  int l, c, k, d, l2, on = 1, v, r, n, i, made = 0, raw = -1, fd;
  char buf[32], b2[8], b3[8];
  struct stat st;
  struct iovec iv[2];
  fd_set rd, wr;
  struct timeval tv;
  static int many[80];
  _kernel_oserror *e;
  printf ("Sockets as descriptors\n");
  l = socket (AF_INET, SOCK_STREAM, 0); c = socket (AF_INET, SOCK_STREAM, 0);
  check (l >= 3 && c >= 3 && l != c, "socket () gives descriptors: 3 or more, and different");
  printf ("  INFO  descriptors %d and %d are the Internet module's sockets %d and %d\n", l, c, __modlib_socket_handle (l), __modlib_socket_handle (c));
  check (fcntl (l, F_GETFL) == O_RDWR, "fcntl F_GETFL: O_RDWR");
  r = ioctl (l, FIONBIO, &on); n = fcntl (l, F_GETFL);
  check (r == 0 && n == (O_RDWR | O_NONBLOCK), "ioctl (FIONBIO) on the descriptor works, and F_GETFL shows O_NONBLOCK");
  r = fcntl (c, F_SETFL, O_NONBLOCK); n = fcntl (c, F_GETFL);
  check (r == 0 && (n & O_NONBLOCK), "fcntl F_SETFL with O_NONBLOCK");
  memset (&st, 0, sizeof st);
  check (fstat (l, &st) == 0 && S_ISSOCK (st.st_mode), "fstat: a socket (S_IFSOCK)");
  setsockopt (l, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
  a = addr_of ("0.0.0.0", 6000);
  check (bind (l, (struct sockaddr *) &a, sizeof a) == 0 && listen (l, 5) == 0, "bind and listen on port 6000");
  a = addr_of ("127.0.0.1", 6000);
  r = connect (c, (struct sockaddr *) &a, sizeof a);
  check (r == 0 || (r < 0 && errno == EINPROGRESS), "connect (non-blocking)");
  check (wait_for (l, 0, 3) == 1, "select on the descriptor: the listener is readable");
  memset (&peer, 0, sizeof peer); pl = sizeof peer;
  k = accept (l, (struct sockaddr *) &peer, &pl);
  check (k >= 3 && k != l && k != c, "accept () gives a descriptor");
  fcntl (k, F_SETFL, O_NONBLOCK);
  check (wait_for (c, 1, 3) == 1, "select: the connecting socket is writable");
  check (write (c, "ping", 4) == 4, "write () on a socket");
  check (wait_for (k, 0, 3) == 1, "select: the accepted socket is readable");
  memset (buf, 0, sizeof buf);
  check (read (k, buf, sizeof buf) == 4 && !memcmp (buf, "ping", 4), "read () on a socket: \"ping\"");
  check (write (c, "abc", 3) == 3, "write \"abc\"");
  check (wait_for (k, 0, 3) == 1, "select: readable again");
  v = -1;
  check (ioctl (k, FIONREAD, &v) == 0 && v == 3, "ioctl (FIONREAD): 3 bytes waiting");
  memset (b2, 0, sizeof b2); memset (b3, 0, sizeof b3);
  iv[0].iov_base = b2; iv[0].iov_len = 2; iv[1].iov_base = b3; iv[1].iov_len = 6;
  check (readv (k, iv, 2) == 3 && !memcmp (b2, "ab", 2) && b3[0] == 'c', "readv () into two buffers: \"ab\" and \"c\"");
  iv[0].iov_base = (void *) "PO"; iv[0].iov_len = 2; iv[1].iov_base = (void *) "NG"; iv[1].iov_len = 2;
  check (writev (k, iv, 2) == 4, "writev () from two buffers");
  check (wait_for (c, 0, 3) == 1, "select: the other side is readable");
  memset (buf, 0, sizeof buf);
  check (read (c, buf, sizeof buf) == 4 && !memcmp (buf, "PONG", 4), "read: \"PONG\"");
  /* select with a descriptor that is not a socket: the screen (1) is always writable (the Internet module is polled, not waited for, when something is ready already) */
  write (k, "Z", 1);
  check (wait_for (c, 0, 3) == 1, "the byte \"Z\" has arrived");
  FD_ZERO (&rd); FD_ZERO (&wr); FD_SET (c, &rd); FD_SET (1, &wr);
  tv.tv_sec = 3; tv.tv_usec = 0;
  n = select (c + 1, &rd, &wr, NULL, &tv);
  check (n == 2 && FD_ISSET (c, &rd) && FD_ISSET (1, &wr), "select with a socket that has data and the screen: both ready");
  check (read (c, buf, sizeof buf) == 1, "read the byte");
  FD_ZERO (&rd); FD_ZERO (&wr); FD_SET (c, &rd); FD_SET (1, &wr);
  tv.tv_sec = 3; tv.tv_usec = 0;
  n = select (c + 1, &rd, &wr, NULL, &tv);
  check (n == 1 && !FD_ISSET (c, &rd) && FD_ISSET (1, &wr), "select with a socket that has nothing and the screen: only the screen is ready");
  /* dup: the Internet module's socket stays open until the last descriptor of it is closed */
  d = dup (k);
  check (d >= 3 && d != k, "dup () of a socket");
  check (close (k) == 0, "close () of the original");
  check (wait_for (c, 0, 1) == 0, "the other side has not seen the end of file: the socket is still open through the copy");
  check (write (d, "x", 1) == 1, "write () through the copy");
  check (wait_for (c, 0, 3) == 1 && read (c, buf, sizeof buf) == 1 && buf[0] == 'x', "the other side reads \"x\"");
  check (close (d) == 0, "close () of the copy");
  check (wait_for (c, 0, 3) == 1 && read (c, buf, sizeof buf) == 0, "now the other side reads the end of file: the Internet module's socket is closed");
  errno = 0;
  check (write (d, "y", 1) == -1 && errno == EBADF, "write () on the closed descriptor: -1 and EBADF");
  check (close (c) == 0 && close (l) == 0, "close () of the other two");
  /* errors */
  errno = 0; r = socketclose (1);
  check (r == -1 && errno == ENOTSOCK, "socketclose (1) (the screen): -1 and ENOTSOCK");
  errno = 0; r = bind (1, (struct sockaddr *) &a, sizeof a);
  check (r == -1 && errno == ENOTSOCK, "bind (1, ...): -1 and ENOTSOCK");
  errno = 0; r = (int) read (97, buf, 1);
  check (r == -1 && errno == EBADF, "read (97) (no descriptor): -1 and EBADF");
  l2 = socket (AF_INET, SOCK_STREAM, 0);
  errno = 0; r = accept (l2, 0, 0);
  check (r == -1 && _inet_err ()[0], "accept () on a socket that does not listen: -1 and the Internet module's message");
  printf ("  INFO  errno %d, _inet_err: \"%s\"\n", errno, _inet_err ());
  close (l2);
  /* a socket made with the raw SWI Socket_Creat becomes a descriptor with __modlib_socket_fd () */
  e = _swix (0x61200, _INR (0, 2) | _OUT (0), AF_INET, SOCK_DGRAM, 0, &raw);
  check (e == NULL && raw >= 0, "the raw SWI Socket_Creat");
  fd = raw >= 0 ? __modlib_socket_fd (raw) : -1;
  v = -1; pl = sizeof v;
  check (fd >= 3 && __modlib_socket_handle (fd) == raw && getsockopt (fd, SOL_SOCKET, SO_TYPE, &v, &pl) == 0 && v == SOCK_DGRAM, "__modlib_socket_fd (): a descriptor for it, and the call works");
  check (close (fd) == 0, "close () of that descriptor closes the Internet module's socket");
  errno = 0;
  check (getsockopt (fd, SOL_SOCKET, SO_TYPE, &v, &pl) == -1 && errno == EBADF, "and it is gone");
  /* the table: how many sockets can be open at once; none is lost when it is full; and many can be opened and closed */
  for (i = 0; i < 80; i++)
    {
      many[i] = socket (AF_INET, SOCK_DGRAM, 0);
      if (many[i] < 0) break;
      made++;
    }
  r = errno;
  printf ("  INFO  %d sockets made, then errno %d (%s)\n", made, r, EN (r));
  check (made >= 10 && made <= 61 && (r == EMFILE || r == ENFILE), "the table (or the Internet module) fills up with EMFILE or ENFILE, after 10 to 61 sockets");
  for (i = 0; i < made; i++) close (many[i]);
  for (i = 0; i < 200; i++) { int s = socket (AF_INET, SOCK_DGRAM, 0); if (s < 0) break; close (s); }
  check (i == 200, "200 times socket () and close (): nothing is lost");
  /* the end of a module: __modlib_closeall () closes the sockets (also a dup'ed one) */
  { int s1 = socket (AF_INET, SOCK_STREAM, 0), s2 = socket (AF_INET, SOCK_DGRAM, 0), s3 = s1 >= 0 ? dup (s1) : -1, cnt;
    cnt = __modlib_closeall ();
    errno = 0;
    check (s1 >= 3 && s2 >= 3 && s3 >= 3 && cnt == 3 && close (s1) == -1 && errno == EBADF, "__modlib_closeall () closes the three descriptors (two sockets, one a dup)"); }
}

/* the interface requests of <sys/ioctl.h> and <net/if.h> on the real Internet module (16.2.0-23): their numbers are those of the RISC OS TCPIPLibs (96 to 99 and 95 for the "get" requests), not 4.4BSD's */
static void ifcheck (void)
{
  char cbuf[1024];
  struct ifconf ifc;
  struct ifreq *p, one;
  struct sockaddr_in *sin;
  int u, r, off, count = 0, lo = 0;
  printf ("Interface requests\n");
  u = socket (AF_INET, SOCK_DGRAM, 0);
  check (u >= 3, "a datagram socket");
  memset (cbuf, 0, sizeof cbuf);
  ifc.ifc_len = sizeof cbuf; ifc.ifc_buf = cbuf;
  r = ioctl (u, SIOCGIFCONF, &ifc);
  printf ("  INFO  SIOCGIFCONF gave %d, length %d\n", r, ifc.ifc_len);
  check (r == 0 && ifc.ifc_len > 0 && ifc.ifc_len <= (int) sizeof cbuf, "ioctl (SIOCGIFCONF): the list of interfaces");
  for (off = 0; off + (int) sizeof (struct ifreq) <= ifc.ifc_len && off + (int) sizeof (struct ifreq) <= (int) sizeof cbuf; off += (int) _SIZEOF_ADDR_IFREQ (*p))
    {
      p = (struct ifreq *) (cbuf + off);
      sin = (struct sockaddr_in *) &p->ifr_addr;
      printf ("  INFO  interface %.16s: family %d, length %d, address %s\n", p->ifr_name, p->ifr_addr.sa_family, p->ifr_addr.sa_len, inet_ntoa (sin->sin_addr));
      if (!strcmp (p->ifr_name, "lo0") && sin->sin_addr.s_addr == inet_addr ("127.0.0.1")) lo = 1;
      count++;
      if (count > 16) break;
    }
  check (count >= 1 && lo, "the list has lo0 with 127.0.0.1");
  memset (&one, 0, sizeof one); strcpy (one.ifr_name, "lo0");
  r = ioctl (u, SIOCGIFADDR, &one); sin = (struct sockaddr_in *) &one.ifr_addr;
  printf ("  INFO  SIOCGIFADDR gave %d, family %d, address %s\n", r, one.ifr_addr.sa_family, inet_ntoa (sin->sin_addr));
  check (r == 0 && sin->sin_addr.s_addr == inet_addr ("127.0.0.1"), "ioctl (SIOCGIFADDR) of lo0: 127.0.0.1");
  memset (&one, 0, sizeof one); strcpy (one.ifr_name, "lo0");
  r = ioctl (u, SIOCGIFFLAGS, &one);
  printf ("  INFO  SIOCGIFFLAGS gave %d, flags &%X\n", r, (unsigned) (unsigned short) one.ifr_flags);
  check (r == 0 && (one.ifr_flags & IFF_UP) && (one.ifr_flags & IFF_LOOPBACK), "ioctl (SIOCGIFFLAGS) of lo0: up and loopback");
  memset (&one, 0, sizeof one); strcpy (one.ifr_name, "lo0");
  r = ioctl (u, SIOCGIFNETMASK, &one); sin = (struct sockaddr_in *) &one.ifr_addr;
  printf ("  INFO  SIOCGIFNETMASK gave %d, mask %s\n", r, inet_ntoa (sin->sin_addr));
  check (r == 0 && sin->sin_addr.s_addr == htonl (0xFF000000), "ioctl (SIOCGIFNETMASK) of lo0: 255.0.0.0");
  memset (&one, 0, sizeof one); strcpy (one.ifr_name, "nonesuch9");
  errno = 0;
  r = ioctl (u, SIOCGIFADDR, &one);
  printf ("  INFO  SIOCGIFADDR of nonesuch9 gave %d, errno %d\n", r, errno);
  check (r == -1, "ioctl (SIOCGIFADDR) of an interface that does not exist: -1");
  check (close (u) == 0, "close ()");
}

/* a socket made with the raw SWI Socket_Creat, turned into a descriptor and closed with close (), with no socket () or accept () before it in this module (the descriptor layer must find the Internet module's
   side of close () by itself: this crashed in the first 16.2.0-23 kit when it was the module's first socket call) */
static void raw (void)
{
  int h = -1, fd, r;
  _kernel_oserror *e;
  printf ("A raw socket as the first socket call\n");
  e = _swix (0x61200, _INR (0, 2) | _OUT (0), AF_INET, SOCK_STREAM, 0, &h);
  check (e == NULL && h >= 0, "the raw SWI Socket_Creat");
  fd = h >= 0 ? __modlib_socket_fd (h) : -1;
  check (fd >= 3 && __modlib_socket_handle (fd) == h, "__modlib_socket_fd (): a descriptor for it");
  r = fd >= 0 ? close (fd) : -2;
  check (r == 0, "close () of the descriptor closes the Internet module's socket");
  errno = 0;
  check (fd >= 0 && close (fd) == -1 && errno == EBADF, "and the descriptor is gone");
}

static void errors (void)
{
  struct sockaddr_in a;
  int s, r, e, v;
  socklen_t vl = sizeof v;
  printf ("Errors\n");
  a = addr_of ("0.0.0.0", 6002);
  errno = 0;
  r = bind (9999, (struct sockaddr *) &a, sizeof a);
  check (r == -1 && errno == EBADF, "bind on 9999 (no descriptor): -1 and EBADF (from the descriptor table: the Internet module is not asked)");
  printf ("  INFO  errno %d\n", errno);
  s = socket (AF_INET, SOCK_STREAM, 0);
  nonblocking (s);
  a = addr_of ("127.0.0.1", 1);
  errno = 0;
  r = connect (s, (struct sockaddr *) &a, sizeof a); e = errno;
  if (r == -1 && e == EINPROGRESS)
    {
      check (wait_for (s, 1, 3) == 1, "connect to a closed port: select says ready");
      v = 0; vl = sizeof v;
      getsockopt (s, SOL_SOCKET, SO_ERROR, &v, &vl);
      check (v == ECONNREFUSED, "connect to port 1: SO_ERROR is ECONNREFUSED");
      printf ("  INFO  SO_ERROR %d\n", v);
    }
  else
    {
      check (r == -1 && e == ECONNREFUSED, "connect to port 1: -1 and ECONNREFUSED");
      printf ("  INFO  connect gave %d errno %d\n", r, e);
    }
  socketclose (s);
}

static void names (void)
{
  struct in_addr in;
  struct hostent *h;
  struct servent *sv;
  char buf[32];
  printf ("Names\n");
  check (inet_addr ("10.1.2.3") == htonl (0x0A010203), "inet_addr (\"10.1.2.3\")");
  check (inet_addr ("127.1") == htonl (0x7F000001) && inet_addr ("0x7f000001") == htonl (0x7F000001) && inet_addr ("300.1.1.1") == INADDR_NONE, "inet_addr: short, hex and bad forms");
  in.s_addr = htonl (0xC6336464);
  check (!strcmp (inet_ntoa (in), "198.51.100.100"), "inet_ntoa");
  check (inet_pton (AF_INET, "8.8.4.4", &in) == 1 && !strcmp (inet_ntop (AF_INET, &in, buf, sizeof buf), "8.8.4.4"), "inet_pton and inet_ntop");
  check (htons (0x1234) == 0x3412 && htonl (0x12345678) == 0x78563412, "htons and htonl");
  h = gethostbyname ("10.20.30.40");
  check (h && !strcmp (inet_ntoa (*(struct in_addr *) h->h_addr_list[0]), "10.20.30.40"), "gethostbyname of a dotted address");
  h = gethostbyname ("localhost");
  if (h) printf ("  INFO  gethostbyname (\"localhost\"): %s, %s\n", h->h_name, inet_ntoa (*(struct in_addr *) h->h_addr_list[0]));
  else printf ("  INFO  gethostbyname (\"localhost\"): NULL, h_errno %d (%s)\n", h_errno, hstrerror (h_errno));
  check (h != NULL && *(unsigned *) h->h_addr_list[0] == inet_addr ("127.0.0.1"), "gethostbyname (\"localhost\") is 127.0.0.1 (the Resolver answers)");
  h = gethostbyname ("no-such-host.invalid");
  printf ("  INFO  gethostbyname (\"no-such-host.invalid\"): %s, h_errno %d\n", h ? "found" : "NULL", h_errno);
  check (h == NULL, "gethostbyname of a name that does not exist: NULL");
  sv = getservbyname ("http", "tcp");
  check (sv && ntohs ((unsigned short) sv->s_port) == 80, "getservbyname (\"http\", \"tcp\") is port 80");
  sv = getservbyname ("syslog", "udp");
  check (sv && ntohs ((unsigned short) sv->s_port) == 514, "getservbyname (\"syslog\", \"udp\") is port 514");
  check (getprotobyname ("udp") && getprotobyname ("udp")->p_proto == 17, "getprotobyname (\"udp\") is 17");
}

static void oslib (void)
{
  char b[64];
  int used, ctx, c;
  unsigned t1, t2;
  os_error *e;
  printf ("OSLib veneers (libOSLib32.a)\n");
  e = xos_set_var_val ("SockHw$Var", (byte const *) "hello", 5, 0, os_VARTYPE_STRING, NULL, NULL);
  check (e == NULL, "xos_set_var_val creates SockHw$Var");
  e = xos_read_var_val ("SockHw$Var", b, sizeof b, 0, os_VARTYPE_STRING, &used, &ctx, NULL);
  check (e == NULL && used == 5 && !memcmp (b, "hello", 5), "xos_read_var_val (X): 5 bytes \"hello\"");
  used = 0;
  c = os_read_var_val ("SockHw$Var", b, sizeof b, 0, os_VARTYPE_STRING, &used, NULL);
  check (used == 5 && !memcmp (b, "hello", 5), "os_read_var_val (non-X): 5 bytes \"hello\"");
  printf ("  INFO  os_read_var_val returned %d (R3: the context)\n", c);
  e = xos_read_var_val ("SockHw$Missing", b, sizeof b, 0, os_VARTYPE_STRING, &used, &ctx, NULL);
  check (e != NULL && e->errnum == 0x124, "xos_read_var_val of a variable that is not there: the error block (&124)");
  if (e) printf ("  INFO  error &%X \"%s\"\n", e->errnum, e->errmess);
  t1 = (unsigned) os_read_monotonic_time (); t2 = (unsigned) os_read_monotonic_time ();
  check (t2 >= t1 && t1 > 1000, "os_read_monotonic_time (non-X): the result is R0, growing");
  os_cli ("Unset SockHw$Var");
  e = xos_read_var_val ("SockHw$Var", b, sizeof b, 0, os_VARTYPE_STRING, &used, &ctx, NULL);
  check (e != NULL, "os_cli (non-X) ran *Unset: the variable is gone");
  os_write0 ("  INFO  os_write0 (a direct SWI)"); os_new_line ();
}

static _kernel_oserror *serve (int secs)
{
  struct sockaddr_in a, peer;
  socklen_t pl = sizeof peer;
  int l, c, on = 1, n, total = 0, t0;
  char buf[256];
  l = socket (AF_INET, SOCK_STREAM, 0);
  setsockopt (l, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
  a = addr_of ("0.0.0.0", 6000);
  if (bind (l, (struct sockaddr *) &a, sizeof a) < 0 || listen (l, 2) < 0) { printf ("bind or listen failed: errno %d: %s\n", errno, _inet_err ()); socketclose (l); return 0; }
  printf ("SockHw_Serve: waiting %d seconds for a client on port 6000\n", secs);
  t0 = os_read_monotonic_time ();
  while (os_read_monotonic_time () - t0 < secs * 100 && wait_for (l, 0, 1) != 1) ;
  if (wait_for (l, 0, 0) != 1) { printf ("no client\n"); socketclose (l); return 0; }
  c = accept (l, (struct sockaddr *) &peer, &pl);
  printf ("client %s:%u (accept gave %d)\n", inet_ntoa (peer.sin_addr), ntohs (peer.sin_port), c);
  while (os_read_monotonic_time () - t0 < secs * 100)
    {
      int i;
      if (wait_for (c, 0, 1) != 1) continue;
      n = (int) recv (c, buf, sizeof buf, 0);
      if (n <= 0) { printf ("recv gave %d (errno %d)\n", n, errno); break; }
      for (i = 0; i < n; i++) buf[i] = (char) toupper ((unsigned char) buf[i]);
      total += n;
      send (c, buf, (size_t) n, 0);
    }
  printf ("echoed %d bytes\n", total);
  socketclose (c); socketclose (l);
  return 0;
}

_kernel_oserror *sh_table (const char *arg_string, int argc, int number, void *pw)
{
  (void) pw;
  switch (number)
    {
    case CMD_SockHw_Test:
      printf ("SockHw_Test\n");
      checks = fails = 0;
      tcp (); udp (); errors (); names (); oslib ();
      printf ("SockHw_Test: %d checks, %d FAILED\n", checks, fails);
      return 0;
    case CMD_SockHw_Raw:
      printf ("SockHw_Raw\n");
      checks = fails = 0;
      raw ();
      printf ("SockHw_Raw: %d checks, %d FAILED\n", checks, fails);
      return 0;
    case CMD_SockHw_Fds:
      printf ("SockHw_Fds\n");
      checks = fails = 0;
      fds ();
      ifcheck ();
      printf ("SockHw_Fds: %d checks, %d FAILED\n", checks, fails);
      return 0;
    case CMD_SockHw_Raise:
      {
        char b[16];
        int used;
        printf ("before the failing non-X call\n");
        os_read_var_val ("SockHw$NotThere", b, sizeof b, 0, os_VARTYPE_STRING, &used, NULL);
        printf ("AFTER: the call came back (the model's behaviour; a real kernel should not get here)\n");
        return 0;
      }
    case CMD_SockHw_Try:
      {
        os_error *e = xos_cli ("SockHw_Raise");
        if (e) printf ("SockHw_Raise gave the error &%X \"%s\"\n", e->errnum, e->errmess);
        else printf ("SockHw_Raise gave no error\n");
        return 0;
      }
    default:
      return serve (argc > 0 ? atoi (arg_string) : 60);
    }
}
