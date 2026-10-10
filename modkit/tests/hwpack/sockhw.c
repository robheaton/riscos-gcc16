/* sockhw.c - the module SockHw of pack/module45: the socket calls of modkit (sys/socket.h ...) on the real Internet module, and the OSLib veneers of libOSLib32.a (X and non-X) on the real kernel.
     *SockHw_Test    everything on the loopback address: TCP (connect, accept, send, recv, socketwrite, socketread, shutdown), UDP (sendto, recvfrom), errors (EBADF, EWOULDBLOCK, ECONNREFUSED), the name
                     functions and the OSLib calls
     *SockHw_Raise   a non-X OSLib function that fails: the error ends the command (OS_GenerateError)
     *SockHw_Try     runs *SockHw_Raise through OS_CLI and prints the error that comes back (an Obey file stops at an error)
     *SockHw_Serve   an echo server on port 6000 for a client on another machine (tests/hw-sockets.py) */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <kernel.h>
#include <swis.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
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

static void errors (void)
{
  struct sockaddr_in a;
  int s, r, e, v;
  socklen_t vl = sizeof v;
  printf ("Errors\n");
  a = addr_of ("0.0.0.0", 6002);
  errno = 0;
  r = bind (9999, (struct sockaddr *) &a, sizeof a);
  check (r == -1 && errno == EBADF && _inet_err ()[0], "bind on socket 9999: -1, EBADF and a message");
  printf ("  INFO  errno %d, _inet_err: \"%s\"\n", errno, _inet_err ());
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
      tcp (); udp (); errors (); names (); oslib ();
      printf ("SockHw_Test: %d checks, %d FAILED\n", checks, fails);
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
