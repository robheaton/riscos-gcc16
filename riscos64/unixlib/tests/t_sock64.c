/* t_sock64.c - poll and select on sockets that have high numbers.  The Internet module numbers its sockets from about 30 upwards, and poll and select of UnixLib 64 handled the numbers up to 31 only: a program
   with a few sockets open (a web client with several connections) had a socket above that reported ready at once.  The program opens 24 UDP sockets first, so that the TCP sockets after them have numbers
   above 31, makes a TCP connection on 127.0.0.1, and checks that poll and select say "not ready" until there is something to read (with the libraries of tool chain 0.14 three of its checks fail on a RISC OS 64 machine), that a connected socket is writable, and that the end of the stream is reported.
   Needs the Internet module: where socket () fails (the build service for Pyromaniac) it prints "skipped" and returns 0.  The SDK of RISC OS 64. */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf ("FAIL %s (line %d, errno %d)\n", #c, __LINE__, errno); } else printf ("ok   %s\n", #c); } while (0)

int main (void)
{
  int dummy[24], i, ls, c, s, on = 1, n;
  struct sockaddr_in a;
  socklen_t len = sizeof a;
  struct pollfd p[2];
  fd_set rd, wr;
  struct timeval tv;
  char buf[8];

  for (i = 0; i < 24; i++)
    {
      dummy[i] = socket (AF_INET, SOCK_DGRAM, 0);
      if (dummy[i] < 0) { printf ("skipped: no Internet module (socket: errno %d)\n", errno); return 0; }
    }
  ls = socket (AF_INET, SOCK_STREAM, 0);
  memset (&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_addr.s_addr = htonl (INADDR_LOOPBACK); a.sin_port = 0;
  CHECK (ls >= 0 && bind (ls, (struct sockaddr *) &a, sizeof a) == 0 && listen (ls, 4) == 0);
  CHECK (getsockname (ls, (struct sockaddr *) &a, &len) == 0 && a.sin_port != 0);
  c = socket (AF_INET, SOCK_STREAM, 0);
  CHECK (c >= 0 && connect (c, (struct sockaddr *) &a, sizeof a) == 0);
  s = accept (ls, NULL, NULL);
  CHECK (s >= 0);
  printf ("sockets: first %d, listening %d, client %d, server side %d\n", dummy[0], ls, c, s);
  CHECK (setsockopt (c, IPPROTO_TCP, TCP_NODELAY, &on, sizeof on) == 0);

  /* poll: the client can be written, there is nothing to read on either socket */
  p[0].fd = c; p[0].events = POLLIN | POLLOUT; p[0].revents = 0;
  p[1].fd = s; p[1].events = POLLIN; p[1].revents = 0;
  n = poll (p, 2, 300);
  CHECK (n == 1 && p[0].revents == POLLOUT && p[1].revents == 0);
  CHECK (write (s, "x", 1) == 1);
  p[0].revents = p[1].revents = 0;
  n = poll (p, 2, 3000);
  CHECK (n >= 1 && (p[0].revents & POLLIN) && !(p[1].revents & POLLIN));
  CHECK (read (c, buf, sizeof buf) == 1 && buf[0] == 'x');

  /* select: not ready until there is data, writable at once */
  FD_ZERO (&rd); FD_SET (c, &rd);
  tv.tv_sec = 0; tv.tv_usec = 300000;
  n = select (c + 1, &rd, NULL, NULL, &tv);
  CHECK (n == 0 && !FD_ISSET (c, &rd));
  FD_ZERO (&wr); FD_SET (c, &wr);
  tv.tv_sec = 0; tv.tv_usec = 300000;
  n = select (c + 1, NULL, &wr, NULL, &tv);
  CHECK (n == 1 && FD_ISSET (c, &wr));
  CHECK (write (s, "yz", 2) == 2);
  FD_ZERO (&rd); FD_SET (c, &rd);
  tv.tv_sec = 3; tv.tv_usec = 0;
  n = select (c + 1, &rd, NULL, NULL, &tv);
  CHECK (n == 1 && FD_ISSET (c, &rd));
  CHECK (read (c, buf, sizeof buf) == 2 && !memcmp (buf, "yz", 2));

  /* the end of the stream is readable */
  close (s);
  p[0].fd = c; p[0].events = POLLIN; p[0].revents = 0;
  n = poll (p, 1, 3000);
  CHECK (n == 1 && (p[0].revents & (POLLIN | POLLHUP)));
  CHECK (read (c, buf, sizeof buf) == 0);

  close (c); close (ls);
  for (i = 0; i < 24; i++) close (dummy[i]);
  printf (fails ? "FAILED: %d failures\n" : "PASSED: %d failures\n", fails);
  return fails != 0;
}
