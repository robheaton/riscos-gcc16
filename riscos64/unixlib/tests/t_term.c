/* t_term.c - termios, poll and select on the console (no key is pressed: only what can be tested without a keyboard). */
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

void cfmakeraw (struct termios *);	/* not in the picolibc header */
static int fails;
#define CHECK(c) do { if (c) printf ("ok   %s\n", #c); else { printf ("FAIL %s\n", #c); fails++; } } while (0)

int main (void)
{
  struct termios t, u;
  struct pollfd p[2];
  fd_set r, w;
  struct timeval tv;
  time_t t0;
  int n;

  CHECK (tcgetattr (0, &t) == 0);
  CHECK ((t.c_lflag & (ECHO | ICANON)) == (ECHO | ICANON));
  u = t;
  cfmakeraw (&u);
  CHECK (tcsetattr (0, TCSANOW, &u) == 0);
  CHECK (tcgetattr (0, &t) == 0);
  CHECK ((t.c_lflag & (ECHO | ICANON)) == 0);
  CHECK (t.c_cc[VMIN] == 1);
  CHECK (tcsetattr (0, TCSADRAIN, &t) == 0);
  CHECK (tcgetattr (7, &t) == -1 && errno == ENOTTY);
  CHECK (tcgetattr (-1, &t) == -1 && errno == EBADF);
  CHECK (cfsetospeed (&t, B9600) == 0 && cfgetospeed (&t) == B9600);

  p[0].fd = 1; p[0].events = POLLOUT; p[1].fd = -1; p[1].events = POLLIN;
  CHECK (poll (p, 2, 0) == 1 && (p[0].revents & POLLOUT) && p[1].revents == 0);
  p[0].fd = 0; p[0].events = POLLIN;
  t0 = time (0);
  n = poll (p, 1, 1000);
  CHECK (n == 0 || n == 1);
  if (n == 0) CHECK (time (0) - t0 >= 0);

  FD_ZERO (&r); FD_ZERO (&w); FD_SET (1, &w);
  tv.tv_sec = 0; tv.tv_usec = 0;
  CHECK (select (2, 0, &w, 0, &tv) == 1 && FD_ISSET (1, &w));
  FD_ZERO (&r); FD_SET (0, &r);
  tv.tv_sec = 0; tv.tv_usec = 200000;
  n = select (1, &r, 0, 0, &tv);
  CHECK (n == 0 || n == 1);
  CHECK (tcflush (0, TCIFLUSH) == 0);
  printf ("%s: %d failures\n", fails ? "FAILED" : "PASSED", fails);
  return fails != 0;
}
