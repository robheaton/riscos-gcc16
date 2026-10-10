/* Sockets in a module (sys/socket.h, netinet/in.h, arpa/inet.h, netdb.h of modkit): a server on the Internet module's SWIs, the error paths, and the name functions. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "kernel.h"
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
  int r;
  memset (&name, 0, sizeof name);
  errno = 0;
  r = bind (99, (struct sockaddr *) &name, sizeof name);
  printf ("bind(99): %d errno %d '%s'\n", r, errno, _inet_err ());
  errno = 0;
  r = (int) send (98, "x", 1, 0);
  printf ("send(98): %d errno %d\n", r, errno);
  errno = 0;
  r = socketclose (97);
  printf ("socketclose(97): %d errno %d\n", r, errno);
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

_kernel_oserror *st_command (const char *arg_string, int argc, int number, void *pw)
{
  (void) arg_string; (void) argc; (void) pw;
  switch (number)
    {
    case CMD_SockTest_Server: return server ();
    case CMD_SockTest_Errors: return errors ();
    default: return names ();
    }
}
