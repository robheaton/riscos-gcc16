/* t_sock.c - sockets.  Part 1 needs no network (names, services, addresses, the tables); part 2 needs the Internet module: a TCP server and client on 127.0.0.1 in one program (non-blocking accept, select, send,
   recv, shutdown, getsockname, getpeername, setsockopt, fcntl, ioctl, poll) and a UDP pair.  Where the Internet SWIs are disabled (the build service for Pyromaniac) part 2 is skipped: socket () fails with ENETDOWN. */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf ("FAIL %s (line %d, errno %d)\n", #c, __LINE__, errno); } else printf ("ok   %s\n", #c); } while (0)

static void names (void)
{
  struct addrinfo hints, *res = NULL, *p;
  struct servent *se;
  struct protoent *pr;
  struct hostent *he;
  struct sockaddr_in sin;
  char host[64], serv[32];
  int n = 0;
  memset (&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  CHECK (getaddrinfo ("127.0.0.1", "8080", &hints, &res) == 0 && res);
  CHECK (res && res->ai_family == AF_INET && res->ai_socktype == SOCK_STREAM && res->ai_protocol == 6 && res->ai_addrlen == sizeof (struct sockaddr_in));
  CHECK (res && ((struct sockaddr_in *) res->ai_addr)->sin_port == htons (8080) && ((struct sockaddr_in *) res->ai_addr)->sin_addr.s_addr == htonl (INADDR_LOOPBACK));
  freeaddrinfo (res);
  hints.ai_socktype = 0;
  CHECK (getaddrinfo ("localhost", "http", &hints, &res) == 0);
  for (p = res; p; p = p->ai_next) n++;
  CHECK (n == 2 && ((struct sockaddr_in *) res->ai_addr)->sin_port == htons (80));
  freeaddrinfo (res);
  hints.ai_flags = AI_PASSIVE;
  CHECK (getaddrinfo (NULL, "9", &hints, &res) == 0 && ((struct sockaddr_in *) res->ai_addr)->sin_addr.s_addr == 0);
  freeaddrinfo (res);
  hints.ai_flags = AI_NUMERICHOST;
  CHECK (getaddrinfo ("not-an-address", NULL, &hints, &res) == EAI_NONAME);
  hints.ai_flags = 0; hints.ai_family = 99;
  CHECK (getaddrinfo ("1.2.3.4", NULL, &hints, &res) == EAI_FAMILY);
  CHECK (gai_strerror (EAI_NONAME)[0] != 0);
  se = getservbyname ("https", "tcp");
  CHECK (se && ntohs ((unsigned short) se->s_port) == 443);
  se = getservbyport (htons (22), "tcp");
  CHECK (se && !strcmp (se->s_name, "ssh"));
  CHECK (getservbyname ("no-such-service", NULL) == NULL);
  pr = getprotobyname ("udp");
  CHECK (pr && pr->p_proto == 17);
  pr = getprotobynumber (6);
  CHECK (pr && !strcmp (pr->p_name, "tcp"));
  he = gethostbyname ("10.1.2.3");
  CHECK (he && he->h_addrtype == AF_INET && he->h_length == 4 && (unsigned char) he->h_addr[0] == 10 && (unsigned char) he->h_addr[3] == 3);
  he = gethostbyname ("localhost");
  CHECK (he && (unsigned char) he->h_addr[0] == 127);
  memset (&sin, 0, sizeof sin);
  sin.sin_family = AF_INET; sin.sin_port = htons (443); sin.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  CHECK (getnameinfo ((struct sockaddr *) &sin, sizeof sin, host, sizeof host, serv, sizeof serv, NI_NUMERICHOST) == 0 && !strcmp (host, "127.0.0.1") && !strcmp (serv, "https"));
  CHECK (getnameinfo ((struct sockaddr *) &sin, sizeof sin, host, sizeof host, serv, sizeof serv, NI_NUMERICHOST | NI_NUMERICSERV) == 0 && !strcmp (serv, "443"));
  CHECK (getnameinfo ((struct sockaddr *) &sin, sizeof sin, host, 4, serv, sizeof serv, NI_NUMERICHOST) == EAI_OVERFLOW);
  errno = 0;
  CHECK (socket (AF_UNIX, SOCK_STREAM, 0) == -1 && errno == EAFNOSUPPORT);
  errno = 0;
  CHECK (send (5, "x", 1, 0) == -1 && errno == EBADF);
}

static void loopback (void)
{
  int ls, cs, as, one = 1, n, flags;
  struct sockaddr_in sin, peer, local;
  socklen_t len;
  char buf[64];
  fd_set rd, wr;
  struct timeval tv;
  struct pollfd pfd;
  errno = 0;
  ls = socket (AF_INET, SOCK_STREAM, 0);
  if (ls < 0 && (errno == ENETDOWN || errno == ENOSYS))
    {
      printf ("SKIP the network part: the Internet module is not usable here (errno %d)\n", errno);
      return;
    }
  CHECK (ls >= 300);
  CHECK (setsockopt (ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one) == 0);
  memset (&sin, 0, sizeof sin);
  sin.sin_family = AF_INET; sin.sin_addr.s_addr = htonl (INADDR_LOOPBACK); sin.sin_port = 0;
  CHECK (bind (ls, (struct sockaddr *) &sin, sizeof sin) == 0);
  CHECK (listen (ls, 4) == 0);
  len = sizeof local;
  CHECK (getsockname (ls, (struct sockaddr *) &local, &len) == 0 && local.sin_family == AF_INET && local.sin_port != 0 && len == sizeof local);
  CHECK (ioctl (ls, FIONBIO, &one) == 0);
  flags = fcntl (ls, F_GETFL);
  CHECK (flags >= 0 && (flags & O_NONBLOCK));
  errno = 0;
  CHECK (accept (ls, NULL, NULL) == -1 && (errno == EWOULDBLOCK || errno == EAGAIN));
  cs = socket (AF_INET, SOCK_STREAM, 0);
  CHECK (cs >= 300 && cs != ls);
  CHECK (connect (cs, (struct sockaddr *) &local, sizeof local) == 0);
  FD_ZERO (&rd);
  FD_SET (ls, &rd);
  tv.tv_sec = 2; tv.tv_usec = 0;
  CHECK (select (ls + 1, &rd, NULL, NULL, &tv) == 1 && FD_ISSET (ls, &rd));
  len = sizeof peer;
  as = accept (ls, (struct sockaddr *) &peer, &len);
  CHECK (as >= 300 && peer.sin_family == AF_INET && peer.sin_addr.s_addr == htonl (INADDR_LOOPBACK));
  CHECK (send (cs, "hello", 5, 0) == 5);
  pfd.fd = as; pfd.events = POLLIN; pfd.revents = 0;
  CHECK (poll (&pfd, 1, 2000) == 1 && (pfd.revents & POLLIN));
  { int avail = 0; CHECK (ioctl (as, FIONREAD, &avail) == 0 && avail == 5); }
  n = (int) recv (as, buf, sizeof buf, 0);
  CHECK (n == 5 && !memcmp (buf, "hello", 5));
  CHECK (write (as, "world!", 6) == 6);
  FD_ZERO (&wr);
  FD_SET (cs, &wr);
  tv.tv_sec = 1;
  CHECK (select (cs + 1, NULL, &wr, NULL, &tv) == 1 && FD_ISSET (cs, &wr));
  n = (int) read (cs, buf, sizeof buf);
  CHECK (n == 6 && !memcmp (buf, "world!", 6));
  len = sizeof peer;
  CHECK (getpeername (cs, (struct sockaddr *) &peer, &len) == 0 && peer.sin_port == local.sin_port);
  CHECK (shutdown (cs, SHUT_WR) == 0);
  CHECK (recv (as, buf, sizeof buf, 0) == 0);
  CHECK (close (cs) == 0 && close (as) == 0 && close (ls) == 0);
  errno = 0;
  CHECK (close (cs) == -1 || errno == EBADF);
  {
    int a, b;
    struct sockaddr_in addr_a, addr_b;
    len = sizeof addr_a;
    a = socket (AF_INET, SOCK_DGRAM, 0);
    b = socket (AF_INET, SOCK_DGRAM, 0);
    memset (&addr_a, 0, sizeof addr_a);
    addr_a.sin_family = AF_INET; addr_a.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    CHECK (a >= 300 && b >= 300 && bind (a, (struct sockaddr *) &addr_a, sizeof addr_a) == 0);
    CHECK (getsockname (a, (struct sockaddr *) &addr_a, &len) == 0);
    CHECK (sendto (b, "ping", 4, 0, (struct sockaddr *) &addr_a, sizeof addr_a) == 4);
    len = sizeof addr_b;
    n = (int) recvfrom (a, buf, sizeof buf, 0, (struct sockaddr *) &addr_b, &len);
    CHECK (n == 4 && !memcmp (buf, "ping", 4) && addr_b.sin_family == AF_INET);
    close (a); close (b);
  }
}

int main (void)
{
  names ();
  loopback ();
  printf (fails ? "FAILED: %d failures\n" : "PASSED: %d failures\n", fails);
  return fails != 0;
}
