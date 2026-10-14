/* modsock.c - sockets, netdb and the stack check in a module (16.2.0-19); sockets as file descriptors (read, write, readv, writev, ioctl, fcntl, dup, close, fstat), <net/if.h>, <sys/param.h> and <sys/errno.h> (16.2.0-23):
   compiles with the headers of the kit and links with nothing undefined. */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <kernel.h>
#include "modsock.h"

_kernel_oserror *modsock_cmd (const char *args, int argc, int number, void *pw)
{
  struct sockaddr_in a;
  struct hostent *h;
  struct ifreq ifr;
  struct iovec iv[2];
  struct stat st;
  fd_set rd;
  struct timeval tv = { 0, 0 };
  char buf[MAXHOSTNAMELEN > 64 ? 64 : MAXHOSTNAMELEN];
  int s, d, on = 1;
  (void) args; (void) argc; (void) number; (void) pw;
  memset (&a, 0, sizeof a);
  a.sin_len = sizeof a; a.sin_family = AF_INET; a.sin_port = htons (6000); a.sin_addr.s_addr = inet_addr ("127.0.0.1");
  s = socket (AF_INET, SOCK_STREAM, 0);
  socketioctl (s, FIONBIO, &on);
  ioctl (s, FIONBIO, &on);                                        /* a socket is a file descriptor: the descriptor calls take it */
  fcntl (s, F_SETFL, fcntl (s, F_GETFL) | O_NONBLOCK);
  connect (s, (struct sockaddr *) &a, sizeof a);
  FD_ZERO (&rd); FD_SET (s, &rd);
  select (s + 1, &rd, NULL, NULL, &tv);
  write (s, "x", 1);
  read (s, buf, sizeof buf);
  iv[0].iov_base = buf; iv[0].iov_len = 1; iv[1].iov_base = buf + 1; iv[1].iov_len = 1;
  writev (s, iv, 2);
  readv (s, iv, 2);
  d = dup (s);
  close (d);
  if (fstat (s, &st) == 0 && S_ISSOCK (st.st_mode)) puts ("a socket");
  memset (&ifr, 0, sizeof ifr);
  strncpy (ifr.ifr_name, "lo0", IFNAMSIZ);
  ioctl (s, SIOCGIFADDR, &ifr);
  d = __modlib_socket_fd (__modlib_socket_handle (s));            /* for code that calls the Socket_ SWIs itself */
  errno = ENOTSOCK;
  h = gethostbyname ("localhost");
  printf ("%s %ld %d %d\n", h ? h->h_name : "-", __modlib_stack_left (), Image__RO_Base != 0, d);
  socketclose (s);
  return 0;
}
