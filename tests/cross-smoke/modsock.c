/* modsock.c - sockets, netdb and the stack check in a module (16.2.0-19): compiles with the headers of the kit and links with nothing undefined. */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <kernel.h>
#include "modsock.h"

_kernel_oserror *modsock_cmd (const char *args, int argc, int number, void *pw)
{
  struct sockaddr_in a;
  struct hostent *h;
  fd_set rd;
  struct timeval tv = { 0, 0 };
  int s, on = 1;
  (void) args; (void) argc; (void) number; (void) pw;
  memset (&a, 0, sizeof a);
  a.sin_len = sizeof a; a.sin_family = AF_INET; a.sin_port = htons (6000); a.sin_addr.s_addr = inet_addr ("127.0.0.1");
  s = socket (AF_INET, SOCK_STREAM, 0);
  socketioctl (s, FIONBIO, &on);
  connect (s, (struct sockaddr *) &a, sizeof a);
  FD_ZERO (&rd); FD_SET (s, &rd);
  select (s + 1, &rd, NULL, NULL, &tv);
  h = gethostbyname ("localhost");
  printf ("%s %ld %d\n", h ? h->h_name : "-", __modlib_stack_left (), Image__RO_Base != 0);
  socketclose (s);
  return 0;
}
